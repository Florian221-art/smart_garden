# API-Vertrag ESP32 ⇄ Server

Version: **1.1** (28.09.2026) · Besitzer: Claude-ESP · Änderungen nur per `[CONTRACT]`-PR (siehe `.claude/CLAUDE.md`).

## 1. Architektur und Netzwerk

```
            WLAN-Hotspot "SmartGarden" (vom Pi aufgespannt, WPA2)
[ESP32 + Sensoren/Aktoren] --HTTP(S) POST alle N s--> [Raspberry Pi 5, 10.42.0.1]
                                                     ├─ Caddy (HTTPS, Reverse Proxy)
                                                     ├─ FastAPI-Server (server/) + KI-Modul (ai/)
                                                     ├─ SQLite-DB
                                                     └─ Dashboard (web/)
[Browser Schüler/Lehrer im Hotspot] --HTTPS--> Pi
```

- Der Pi spannt ein **eigenes WLAN** auf (NetworkManager-Hotspot, Standard-IP des Pi: `10.42.0.1`). ESP32 und Laptops verbinden sich damit. Unabhängig vom Hackathon-WLAN.
- Server-URL für den ESP: Phase 1 `http://10.42.0.1:8000`, ab Phase 3 `https://10.42.0.1`.
- Der ESP baut **nur ausgehende** Verbindungen auf (kein Webserver auf dem ESP). Befehle an den ESP kommen in der **Antwort** auf den Messwert-POST.
- Der ESP bewässert bei Bedarf **selbstständig** (funktioniert auch, wenn der Server ausfällt). Der Server kann Grenzwerte ändern und manuelles Gießen anstoßen.

## 2. Endpunkt: Messwerte senden

`POST /api/v1/readings`

Header:

```
Content-Type: application/json
X-API-Key: <Geräteschlüssel>
```

Body (alle Messfelder dürfen `null` sein, wenn Sensor fehlt/defekt):

```json
{
  "device_id": "esp32-kuebel-01",
  "fw_version": "0.1.0",
  "seq": 1234,
  "uptime_s": 3600,
  "rssi_dbm": -61,
  "soil_moisture_pct": 42.5,
  "soil_moisture_raw": 2100,
  "light_pct": 70.1,
  "light_raw": 1200,
  "air_temp_c": 22.0,
  "air_humidity_pct": 55.0,
  "tank_remaining_ml": 1240,
  "water_level_pct": 82.7,
  "pump_on_s_since_last": 0.0,
  "auto_water_triggered": false,
  "errors": []
}
```

| Feld | Typ | Bedeutung |
|---|---|---|
| `device_id` | string, max 32, `[a-z0-9-]` | Muss zum API-Key passen |
| `fw_version` | string | Firmware-Version |
| `seq` | int ≥ 0 | Laufende Nummer seit Boot (Duplikate/Lücken erkennen) |
| `uptime_s` | int | Sekunden seit Boot |
| `rssi_dbm` | int | WLAN-Signalstärke |
| `soil_moisture_pct` | float 0–100 / null | Bodenfeuchte (kalibriert) |
| `soil_moisture_raw` | int 0–4095 / null | ADC-Rohwert |
| `light_pct` | float 0–100 / null | Helligkeit relativ (0 = dunkel) |
| `light_raw` | int 0–4095 / null | ADC-Rohwert |
| `air_temp_c` | float / null | Lufttemperatur (DHT11, ±2 °C) |
| `air_humidity_pct` | float / null | Luftfeuchte (DHT11, ±5 %) |
| `tank_remaining_ml` | float ≥ 0 | **Geschätzte** Restmenge im Tank (siehe Abschnitt 3) |
| `water_level_pct` | float 0–100 | **Geschätzter** Füllstand = `tank_remaining_ml / tank_capacity_ml × 100` |
| `pump_on_s_since_last` | float ≥ 0 | Pumpenlaufzeit seit letztem erfolgreichen POST |
| `auto_water_triggered` | bool | ESP hat seit letztem POST selbst gegossen |
| `errors` | string[] | Fehlercodes, z. B. `"dht_read_failed"`, `"soil_out_of_range"`, `"tank_empty"` |

Den **Zeitstempel setzt der Server** (`received_at`, UTC). Der ESP sendet keine Uhrzeit.

### Antwort 200

```json
{
  "ok": true,
  "commands": {
    "pump_run_s": 0,
    "buzzer": false,
    "identify": false,
    "tank_refilled": false
  },
  "config": {
    "interval_s": 15,
    "moisture_min_pct": 30,
    "moisture_target_pct": 55,
    "auto_water": true,
    "max_pump_s_per_run": 5,
    "pump_cooldown_s": 300,
    "max_pump_s_per_day": 60,
    "buzzer_enabled": true,
    "tank_capacity_ml": 1500,
    "pump_flow_ml_per_s": 20,
    "tank_low_pct": 20
  }
}
```

- `commands` werden **genau einmal** ausgeliefert (danach auf Server als erledigt markieren).
- `pump_run_s`: manuelles Gießen (vom Admin im Dashboard), ESP begrenzt hart auf `max_pump_s_per_run`.
- `identify`: LED-Bar blinkt 5 s (Gerät im Raum finden).
- `tank_refilled`: Tank wurde aufgefüllt → ESP setzt `tank_remaining_ml = tank_capacity_ml`.
- `config` wird jedes Mal komplett mitgeschickt; ESP übernimmt die Werte (mit eigenen Sicherheitsgrenzen).

### Fehler

| Code | Wann | Body |
|---|---|---|
| 401 | API-Key fehlt/falsch oder passt nicht zu `device_id` | `{"ok": false, "error": "unauthorized"}` |
| 422 | Validierung fehlgeschlagen | `{"ok": false, "error": "validation", "detail": ...}` |
| 429 | Mehr als 1 Request/2 s pro Gerät | `{"ok": false, "error": "rate_limited"}` |

Der ESP wertet nur `200` aus; bei allem anderen: loggen und beim nächsten Intervall erneut senden.

## 3. Tank-Füllstand ohne Sensor (Schätzung)

Es gibt keinen Füllstandssensor. Der Füllstand wird aus der Pumpenlaufzeit geschätzt:

```
tank_remaining_ml -= pump_laufzeit_s × pump_flow_ml_per_s
```

- **Der ESP ist maßgeblich:** Er rechnet mit, speichert den Wert dauerhaft im Flash (überlebt Neustart) und meldet ihn bei jedem POST.
- Nachfüllen: Button „Tank aufgefüllt“ im Dashboard → Command `tank_refilled` (alternativ: BOOT-Taste am ESP 3 s halten).
- `tank_capacity_ml` und `pump_flow_ml_per_s` stellt der Admin im Dashboard ein. Durchfluss einmal auspessen: Pumpe 10 s laufen lassen, Menge im Messbecher ablesen, durch 10 teilen.
- Unter `tank_low_pct` → Warnung (Server-Alert, LED-Bar, Summer). Bei ≤ 5 % stoppt der ESP die automatische Bewässerung (Trockenlaufschutz) und meldet `tank_empty`.
- Der Server berechnet den Wasserverbrauch über `pump_on_s_since_last × pump_flow_ml_per_s`.

## 4. Warnungen

Warnungen (Alerts) erzeugt der Server aus Grenzwerten, Fehlercodes und KI-Ergebnis. Texte **nur als Schlüssel** (`message_key`), Übersetzung im Frontend.

## 5. Test ohne Hardware

`tools/fake_esp.py` (Claude-ESP) sendet realistische Fake-Messwerte im Vertragsformat, wertet die Antwort aus (inkl. `pump_run_s`, `tank_refilled`) und simuliert den Tank:

```
python tools/fake_esp.py --url http://localhost:8000 --key <api-key> --interval 2
```
