#!/usr/bin/env python3
"""
fake_esp.py – simuliert den ESP32 des Smart-Garden-Kübels.

Sendet Messwerte exakt nach .claude/api-contract.md (v1.2) an den Server,
wertet die Antwort aus (commands, config, demo) und verhält sich wie die echte
Firmware: Bodenfeuchte trocknet aus, Auto-Bewässerung mit Sicherheitsgrenzen,
geschätzter Tank, Tag/Nacht-Verlauf für Licht und Temperatur.

Nur Python-Standardbibliothek – keine Installation nötig.

Beispiele:
    python tools/fake_esp.py --url http://localhost:8000 --key test-key
    python tools/fake_esp.py --url http://localhost:8000 --key test-key --interval 2 --speed 60
    python tools/fake_esp.py --url http://localhost:8000 --key test-key --anomaly heat --count 20
    python tools/fake_esp.py --dry-run --count 3        # nur JSON ausgeben, nichts senden

--speed 60 bedeutet: 1 echte Sekunde = 60 simulierte Sekunden
(damit man Austrocknen und Tag/Nacht in wenigen Minuten sieht).
"""

from __future__ import annotations

import argparse
import json
import math
import random
import ssl
import sys
import time
import urllib.error
import urllib.request

FW_VERSION = "sim-0.3.0"

DEFAULT_CONFIG = {
    "interval_s": 15,
    "moisture_min_pct": 30,
    "moisture_target_pct": 55,
    "auto_water": True,
    "max_pump_s_per_run": 5,
    "pump_cooldown_s": 300,
    "max_pump_s_per_day": 60,
    "buzzer_enabled": True,
    "tank_capacity_ml": 1500,
    "pump_flow_ml_per_s": 20,
    "tank_low_pct": 20,
}

ALLOWED_OVERRIDES = {"soil_moisture_pct", "light_pct", "air_temp_c", "air_humidity_pct", "water_level_pct"}
MAX_DEMO_S = 600
HARD_MIN_COOLDOWN_S = 10  # Mindestpause zwischen zwei Pumpenläufen (gilt für alle Starts, wie Firmware)
TANK_EMPTY_PCT = 5.0
# Wie viel Prozent Bodenfeuchte ein Milliliter Wasser bringt (grob für einen kleinen Kübel)
MOISTURE_PCT_PER_ML = 0.15


def clamp(v: float, lo: float, hi: float) -> float:
    return max(lo, min(hi, v))


class FakeEsp:
    def __init__(self, args: argparse.Namespace) -> None:
        self.args = args
        self.config = dict(DEFAULT_CONFIG)
        self.seq = 0
        self.start = time.monotonic()
        self.sim_time_s = args.start_hour * 3600.0  # simulierte Tageszeit
        self.soil = args.start_moisture
        self.tank_ml = float(self.config["tank_capacity_ml"])
        self.pump_on_since_last = 0.0
        self.pump_today_s = 0.0
        self.last_pump_sim_s = -1e9
        self.auto_watered = False
        self.pump_running = False
        # Demo
        self.demo_overrides: dict = {}
        self.demo_errors: list[str] = []
        self.demo_until = 0.0
        self.rng = random.Random(args.seed)

    # ------------------------------------------------------------------ Physik
    def advance(self, real_dt: float) -> None:
        sim_dt = real_dt * self.args.speed
        self.sim_time_s += sim_dt
        hour = (self.sim_time_s / 3600.0) % 24
        # Verdunstung: stärker bei Wärme und Licht
        temp = self._true_temp(hour)
        light = self._true_light(hour)
        evaporation_per_h = 1.2 + 0.08 * max(0.0, temp - 15) + 0.01 * light
        self.soil = clamp(self.soil - evaporation_per_h * sim_dt / 3600.0, 0, 100)
        # neuer Tag -> Tageszähler zurücksetzen
        if int((self.sim_time_s - sim_dt) // 86400) != int(self.sim_time_s // 86400):
            self.pump_today_s = 0.0

    def _true_light(self, hour: float) -> float:
        # Sonne zwischen 6 und 20 Uhr
        if 6 <= hour <= 20:
            base = math.sin(math.pi * (hour - 6) / 14) * 85
        else:
            base = 1.0
        return clamp(base + self.rng.gauss(0, 2), 0, 100)

    def _true_temp(self, hour: float) -> float:
        base = 17 + 6 * math.sin(math.pi * (hour - 9) / 12)
        if self.args.anomaly == "heat" and self.seq > 5:
            base += 15
        return round(base + self.rng.gauss(0, 0.3), 1)

    def _true_humidity(self, temp: float) -> float:
        return clamp(75 - (temp - 15) * 1.8 + self.rng.gauss(0, 1.5), 15, 95)

    # ------------------------------------------------------------------ Tank
    def real_tank_pct(self) -> float:
        cap = self.config["tank_capacity_ml"] or 1
        return clamp(self.tank_ml / cap * 100, 0, 100)

    def reported_tank_pct(self) -> float:
        """Gemeldeter Füllstand: im Demo-Modus der Demo-Wert (wird nicht gespeichert)."""
        if self.demo_active() and "water_level_pct" in self.demo_overrides:
            return clamp(float(self.demo_overrides["water_level_pct"]), 0, 100)
        return self.real_tank_pct()

    def tank_is_empty(self) -> bool:
        """Trockenlaufschutz wie Firmware: min(echt, Demo) – Demo macht nie 'voller'."""
        return min(self.real_tank_pct(), self.reported_tank_pct()) <= TANK_EMPTY_PCT

    # ------------------------------------------------------------------ Pumpe
    def run_pump(self, seconds: float, reason: str) -> None:
        cfg = self.config
        if not seconds > 0:
            return
        if self.tank_is_empty():
            log(f"PUMPE blockiert ({reason}): Tank leer (Trockenlaufschutz)")
            return
        if self.sim_time_s - self.last_pump_sim_s < HARD_MIN_COOLDOWN_S:
            log(f"PUMPE blockiert ({reason}): Mindestpause {HARD_MIN_COOLDOWN_S} s")
            return
        seconds = min(seconds, cfg["max_pump_s_per_run"], 15.0)
        seconds = min(seconds, max(0.0, min(cfg["max_pump_s_per_day"], 300.0) - self.pump_today_s))
        if seconds <= 0:
            log(f"PUMPE blockiert ({reason}): Tageslimit erreicht")
            return
        ml = min(seconds * cfg["pump_flow_ml_per_s"], self.tank_ml)
        self.tank_ml -= ml
        self.soil = clamp(self.soil + ml * MOISTURE_PCT_PER_ML, 0, 100)
        self.pump_on_since_last += seconds
        self.pump_today_s += seconds
        self.last_pump_sim_s = self.sim_time_s
        log(f"PUMPE {seconds:.1f} s ({reason}) -> {ml:.0f} ml, Tank {self.tank_ml:.0f} ml")

    def auto_water(self, soil_value: float | None) -> None:
        cfg = self.config
        if not cfg["auto_water"] or soil_value is None:
            return
        if soil_value >= cfg["moisture_min_pct"]:
            return
        if self.sim_time_s - self.last_pump_sim_s < cfg["pump_cooldown_s"]:
            return
        self.run_pump(cfg["max_pump_s_per_run"], "auto")
        self.auto_watered = True

    # ------------------------------------------------------------------ Demo
    def demo_active(self) -> bool:
        return time.monotonic() < self.demo_until and (bool(self.demo_overrides) or bool(self.demo_errors))

    def apply_demo(self, demo: dict | None) -> None:
        if not demo:
            if self.demo_active():
                log("DEMO aus")
            self.demo_overrides, self.demo_errors, self.demo_until = {}, [], 0.0
            return
        ttl = clamp(float(demo.get("expires_in_s", 120)), 0, MAX_DEMO_S)
        overrides = {k: v for k, v in (demo.get("overrides") or {}).items() if k in ALLOWED_OVERRIDES}
        errors = list(demo.get("force_errors") or [])
        if overrides != self.demo_overrides or errors != self.demo_errors:
            log(f"DEMO an: overrides={overrides} errors={errors} ({ttl:.0f} s)")
        self.demo_overrides, self.demo_errors = overrides, errors
        self.demo_until = time.monotonic() + ttl

    # ------------------------------------------------------------------ Messung
    def build_reading(self) -> dict:
        hour = (self.sim_time_s / 3600.0) % 24
        temp = self._true_temp(hour)
        values = {
            "soil_moisture_pct": round(clamp(self.soil + self.rng.gauss(0, 0.4), 0, 100), 1),
            "light_pct": round(self._true_light(hour), 1),
            "air_temp_c": temp,
            "air_humidity_pct": round(self._true_humidity(temp), 1),
        }
        errors: list[str] = []
        overridden: list[str] = []

        if self.args.anomaly == "dht" and self.seq > 5:
            values["air_temp_c"] = values["air_humidity_pct"] = None
            errors.append("dht_read_failed")

        if self.demo_active():
            for k, v in self.demo_overrides.items():
                if k in values:
                    values[k] = float(v)
                    overridden.append(k)
            if "water_level_pct" in self.demo_overrides:
                overridden.append("water_level_pct")
            for e in self.demo_errors:
                errors.append(e)
                if e.startswith("dht"):
                    values["air_temp_c"] = values["air_humidity_pct"] = None
                elif e.startswith("soil"):
                    values["soil_moisture_pct"] = None
                elif e.startswith("light"):
                    values["light_pct"] = None
        elif self.demo_until:
            log("DEMO abgelaufen -> echte Werte")
            self.demo_overrides, self.demo_errors, self.demo_until = {}, [], 0.0

        # Auto-Bewässerung nutzt (ggf. überschriebene) Werte – wie die Firmware
        self.auto_water(values["soil_moisture_pct"])

        cap = self.config["tank_capacity_ml"] or 1
        level = round(self.reported_tank_pct(), 1)
        if self.tank_is_empty():
            errors.append("tank_empty")

        soil_raw = None if values["soil_moisture_pct"] is None else int(3300 - self.soil * 20)
        light_raw = None if values["light_pct"] is None else int(4095 - values["light_pct"] * 38)

        reading = {
            "device_id": self.args.device_id,
            "fw_version": FW_VERSION,
            "seq": self.seq,
            "uptime_s": int(time.monotonic() - self.start),
            "rssi_dbm": int(-55 + self.rng.gauss(0, 4)),
            "soil_moisture_pct": values["soil_moisture_pct"],
            "soil_moisture_raw": soil_raw,
            "light_pct": values["light_pct"],
            "light_raw": light_raw,
            "air_temp_c": values["air_temp_c"],
            "air_humidity_pct": values["air_humidity_pct"],
            "tank_remaining_ml": round(cap * level / 100 if self.demo_active() else self.tank_ml, 1),
            "water_level_pct": level,
            "pump_on_s_since_last": round(self.pump_on_since_last, 1),
            "pump_running": False,
            "auto_water_triggered": self.auto_watered,
            "errors": sorted(set(errors)),
            "demo_overrides": sorted(set(overridden)),
        }
        self.seq += 1
        return reading

    # ------------------------------------------------------------------ Antwort
    def handle_response(self, body: dict) -> None:
        # erfolgreich gesendet -> Zähler zurücksetzen
        self.pump_on_since_last = 0.0
        self.auto_watered = False

        cfg = body.get("config") or {}
        for k, v in cfg.items():
            if k in self.config and v is not None:
                self.config[k] = v
        cmds = body.get("commands") or {}
        if cmds.get("tank_refilled"):
            self.tank_ml = float(self.config["tank_capacity_ml"])
            log("BEFEHL tank_refilled -> Tank voll")
        if cmds.get("pump_run_s"):
            self.run_pump(float(cmds["pump_run_s"]), "manuell")
        if cmds.get("buzzer"):
            log("BEFEHL buzzer -> piep")
        if cmds.get("identify"):
            log("BEFEHL identify -> LED-Bar blinkt")
        self.apply_demo(body.get("demo"))


def log(msg: str) -> None:
    print(f"[{time.strftime('%H:%M:%S')}] {msg}", flush=True)


def post(url: str, key: str, payload: dict, insecure: bool, timeout: float = 5.0) -> tuple[int, dict | None]:
    data = json.dumps(payload).encode()
    req = urllib.request.Request(
        url.rstrip("/") + "/api/v1/readings",
        data=data,
        method="POST",
        headers={"Content-Type": "application/json", "X-API-Key": key},
    )
    ctx = ssl._create_unverified_context() if insecure else None
    try:
        with urllib.request.urlopen(req, timeout=timeout, context=ctx) as resp:
            raw = resp.read()
            return resp.status, (json.loads(raw) if raw else None)
    except urllib.error.HTTPError as e:
        try:
            return e.code, json.loads(e.read() or b"null")
        except ValueError:
            return e.code, None


def main() -> int:
    p = argparse.ArgumentParser(description="Simuliert den Smart-Garden-ESP32 (API-Vertrag v1.2).")
    p.add_argument("--url", default="http://localhost:8000", help="Basis-URL des Servers")
    p.add_argument("--key", default="dev-key", help="API-Key des Geräts (Header X-API-Key)")
    p.add_argument("--device-id", default="esp32-kuebel-01")
    p.add_argument("--interval", type=float, default=None, help="Sendeintervall in s (Standard: interval_s aus Server-Config)")
    p.add_argument("--speed", type=float, default=1.0, help="Zeitraffer: simulierte Sekunden pro echter Sekunde")
    p.add_argument("--count", type=int, default=0, help="Anzahl Nachrichten (0 = endlos)")
    p.add_argument("--start-moisture", type=float, default=45.0)
    p.add_argument("--start-hour", type=float, default=10.0, help="simulierte Startuhrzeit")
    p.add_argument("--anomaly", choices=["none", "heat", "dht"], default="none", help="ab Nachricht 6 Anomalie einbauen")
    p.add_argument("--seed", type=int, default=None)
    p.add_argument("--insecure", action="store_true", help="HTTPS-Zertifikat nicht prüfen (selbstsigniert)")
    p.add_argument("--dry-run", action="store_true", help="nichts senden, nur JSON ausgeben")
    args = p.parse_args()

    esp = FakeEsp(args)
    log(f"Fake-ESP '{args.device_id}' -> {args.url}  (speed x{args.speed}, anomaly={args.anomaly})")
    sent = 0
    last = time.monotonic()
    try:
        while args.count == 0 or sent < args.count:
            now = time.monotonic()
            esp.advance(now - last)
            last = now
            reading = esp.build_reading()

            if args.dry_run:
                print(json.dumps(reading, indent=2, ensure_ascii=False))
                esp.handle_response({})
            else:
                try:
                    status, body = post(args.url, args.key, reading, args.insecure)
                except (urllib.error.URLError, OSError) as e:
                    log(f"Server nicht erreichbar: {e}")
                    status, body = 0, None
                demo = " [DEMO]" if reading["demo_overrides"] else ""
                log(
                    f"#{reading['seq']} -> {status}{demo}  Boden {reading['soil_moisture_pct']} %  "
                    f"Licht {reading['light_pct']} %  Temp {reading['air_temp_c']} °C  "
                    f"LF {reading['air_humidity_pct']} %  Tank {reading['water_level_pct']} %"
                    + (f"  Fehler {reading['errors']}" if reading["errors"] else "")
                )
                if status == 200 and isinstance(body, dict):
                    esp.handle_response(body)
                elif status:
                    log(f"Antwort {status}: {body}")
            sent += 1
            if args.count and sent >= args.count:
                break
            time.sleep(args.interval if args.interval is not None else esp.config["interval_s"])
    except KeyboardInterrupt:
        log("beendet")
    return 0


if __name__ == "__main__":
    sys.exit(main())
