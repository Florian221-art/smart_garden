# API-Vertrag ESP32 ⇄ Server ⇄ KI-Modul

Version: **1.0** (28.09.2026) · Besitzer: Claude-ESP · Änderungen nur per `[CONTRACT]`-PR (siehe `CLAUDE.md`).

## 1. Architektur

```
[ESP32 + Sensoren/Aktoren] --HTTP(S) POST alle N s--> [Raspberry Pi 5]
                                                     ├─ Caddy (HTTPS, Reverse Proxy)
                                                     ├─ FastAPI-Server (server/)  ──import──> KI-Modul (ai/)
                                                     ├─ SQLite-DB
                                                     └─ Dashboard (web/, statisch, von FastAPI ausgeliefert)
[Browser Schüler/Lehrer] --HTTPS--> Pi
```

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
  "water_level_pct": null,
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
| `water_level_pct` | float / null | Tankfüllstand, **aktuell kein Sensor → immer null** |
| `pump_on_s_since_last` | float ≥ 0 | Pumpenlaufzeit seit letztem erfolgreichen POST |
| `auto_water_triggered` | bool | ESP hat seit letztem POST selbst gegossen |
| `errors` | string[] | Fehlercodes, z. B. `"dht_read_failed"`, `"soil_out_of_range"` |

Der **Zeitstempel setzt der Server** (`received_at`, UTC). Der ESP sendet keine Uhrzeit.

### Antwort 200

```json
{
  "ok": true,
  "commands": {
    "pump_run_s": 0,
    "buzzer": false,
    "identify": false
  },
  "config": {
    "interval_s": 15,
    "moisture_min_pct": 30,
    "moisture_target_pct": 55,
    "auto_water": true,
    "max_pump_s_per_run": 5,
    "pump_cooldown_s": 300,
    "max_pump_s_per_day": 60,
    "buzzer_enabled": true
  }
}
```

- `commands` werden **genau einmal** ausgeliefert (danach auf Server als erledigt markieren).
- `pump_run_s`: manuelles Gießen (vom Admin im Dashboard), ESP begrenzt hart auf `max_pump_s_per_run`.
- `identify`: LED-Bar blinkt 5 s (Gerät im Raum finden).
- `config` wird jedes Mal komplett mitgeschickt; ESP übernimmt die Werte (mit eigenen Sicherheitsgrenzen).

### Fehler

| Code | Wann | Body |
|---|---|---|
| 401 | API-Key fehlt/falsch oder passt nicht zu `device_id` | `{"ok": false, "error": "unauthorized"}` |
| 422 | Validierung fehlgeschlagen | `{"ok": false, "error": "validation", "detail": ...}` |
| 429 | Mehr als 1 Request/2 s pro Gerät | `{"ok": false, "error": "rate_limited"}` |

Der ESP wertet nur `200` aus; bei allem anderen: loggen und beim nächsten Intervall erneut senden.

## 3. Werte, die der Server selbst ableitet

- Wasserverbrauch: `ml = pump_on_s × PUMP_FLOW_ML_PER_S` (Konstante im Server-Config, zunächst **20 ml/s**, wird gemessen).
- Warnungen (Alerts) erzeugt der Server aus Grenzwerten + KI-Ergebnis. Texte **nur als Schlüssel** (`message_key`), Übersetzung im Frontend.

## 4. KI-Modul (ai/) – Python-Schnittstelle

Der Server importiert das Modul direkt (kein eigener Dienst):

```python
from ai.garden_ai import analyze

result = analyze(readings, config)
```

- `readings`: `list[dict]`, aufsteigend nach Zeit, max. 7 Tage. Jedes dict hat die Felder aus Abschnitt 2 **plus** `"received_at"` (ISO-8601-String, UTC).
- `config`: das `config`-Objekt aus Abschnitt 2.
- Laufzeit < 1 s auf dem Pi 5; keine Netzwerkzugriffe; wirft keine Exceptions (bei zu wenig Daten → Felder `null`).

Rückgabe:

```json
{
  "model_version": "0.1.0",
  "generated_at": "2026-09-28T14:00:00Z",
  "water_forecast": {
    "hours_until_dry": 18.5,
    "recommend_water_now": false,
    "confidence": 0.7
  },
  "anomalies": [
    {"field": "air_temp_c", "received_at": "2026-09-28T13:55:00Z", "value": 35.2, "score": 4.1, "message_key": "anomaly.temp_spike"}
  ],
  "stress": {
    "score": 35,
    "level": "ok",
    "reasons": ["soil_dry"]
  },
  "water_saved_ml_estimate": 1200
}
```

- `stress.level`: `"ok" | "warn" | "critical"`
- `stress.reasons` und `message_key` sind Übersetzungsschlüssel. Liste aller Schlüssel: `ai/MESSAGE_KEYS.md` (pflegt Claude-ESP).
- Bis das echte Modell fertig ist, liefert ein **Stub** gültige Dummy-Werte mit identischer Struktur.

## 5. Test ohne Hardware

`tools/fake_esp.py` (Claude-ESP) sendet realistische Fake-Messwerte im Vertragsformat an eine beliebige URL:

```
python tools/fake_esp.py --url http://localhost:8000 --key <api-key> --interval 2
```
