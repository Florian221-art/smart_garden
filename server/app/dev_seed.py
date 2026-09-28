"""NUR ENTWICKLUNG: fuellt die DB mit simuliertem Verlauf, damit die Diagramme
im Dashboard etwas zeigen (fake_esp.py sendet in Echtzeit, der Server setzt den
Zeitstempel - Verlauf ueber Stunden/Tage entsteht damit nicht sofort).

    python -m app.dev_seed esp32-kuebel-01 --hours 48

Legt das Geraet an, falls es fehlt (API-Key wird dann ausgegeben).
Simuliert: Tag/Nacht (Licht, Temperatur), austrocknende Erde mit
Auto-Bewaesserung, Tankverbrauch, ein kurzes Demo-Fenster und einen DHT-Ausfall.
Nicht auf dem Pi in der echten Datenbank ausfuehren.
"""
from __future__ import annotations

import argparse
import datetime as dt
import math
import random

from .cli import create_device
from .db import SessionLocal, init_db
from .models import Device, DeviceConfig, Reading


def seed(device_id: str, hours: int, interval_s: int, seed_value: int) -> int:
    init_db()
    db = SessionLocal()
    try:
        if db.get(Device, device_id) is None:
            db.close()
            create_device(device_id, label="Dev-Seed")
            db = SessionLocal()
        cfg = db.get(DeviceConfig, device_id)
        flow = cfg.pump_flow_ml_per_s if cfg else 20.0
        cap = cfg.tank_capacity_ml if cfg else 1500.0
        m_min = cfg.moisture_min_pct if cfg else 30.0

        rnd = random.Random(seed_value)
        now = dt.datetime.now(dt.timezone.utc).replace(tzinfo=None)
        t = now - dt.timedelta(hours=hours)
        moisture, tank, seq, uptime = 52.0, cap * 0.95, 0, 0
        demo_start = now - dt.timedelta(hours=3)
        dht_fail = (now - dt.timedelta(hours=hours * 0.4), now - dt.timedelta(hours=hours * 0.4 - 0.3))
        n = 0
        while t <= now:
            hour = t.hour + t.minute / 60 + 2  # grob Ortszeit (UTC+2)
            day = max(0.0, math.sin((hour - 6) / 24 * 2 * math.pi))  # 0 nachts, 1 mittags
            light = min(100.0, max(0.0, 85 * day + rnd.gauss(0, 2)))
            temp = 14 + 10 * day + rnd.gauss(0, 0.3)
            hum = 75 - 25 * day + rnd.gauss(0, 1.5)
            moisture -= interval_s / 3600 * (1.0 + 2.6 * day)  # tagsueber schneller
            pump_s, auto = 0.0, False
            if moisture < m_min and tank > cap * 0.05:
                pump_s, auto = 5.0, True
                moisture += 22 + rnd.gauss(0, 2)
                tank = max(0.0, tank - pump_s * flow)

            overrides: list[str] = []
            errors: list[str] = []
            soil_v, temp_v, hum_v = moisture, temp, hum
            if demo_start <= t <= demo_start + dt.timedelta(minutes=10):
                overrides = ["soil_moisture_pct"]
                soil_v = 12.0
            if dht_fail[0] <= t <= dht_fail[1]:
                errors.append("dht_read_failed")
                temp_v = hum_v = None

            db.add(Reading(
                device_id=device_id, received_at=t, fw_version="seed-0.1.0",
                seq=seq, uptime_s=uptime, rssi_dbm=int(-58 + rnd.gauss(0, 3)),
                soil_moisture_pct=round(min(100, max(0, soil_v + rnd.gauss(0, 0.4))), 1),
                soil_moisture_raw=int(3200 - soil_v * 18),
                light_pct=round(light, 1), light_raw=int(4095 - light * 35),
                air_temp_c=None if temp_v is None else round(temp_v, 1),
                air_humidity_pct=None if hum_v is None else round(min(100, hum_v), 1),
                tank_remaining_ml=round(tank, 1), water_level_pct=round(tank / cap * 100, 1),
                pump_on_s_since_last=pump_s, pump_running=False, auto_water_triggered=auto,
                errors=errors, demo_overrides=overrides,
            ))
            seq += 1
            uptime += interval_s
            n += 1
            t += dt.timedelta(seconds=interval_s)
        db.commit()
        return n
    finally:
        db.close()


def main() -> None:
    ap = argparse.ArgumentParser(description="Dev: simulierten Verlauf einspielen")
    ap.add_argument("device_id")
    ap.add_argument("--hours", type=int, default=48)
    ap.add_argument("--interval", type=int, default=60, help="Sekunden zwischen Messwerten")
    ap.add_argument("--seed", type=int, default=1)
    a = ap.parse_args()
    n = seed(a.device_id, a.hours, a.interval, a.seed)
    print(f"{n} Messwerte fuer '{a.device_id}' eingespielt ({a.hours} h).")


if __name__ == "__main__":
    main()
