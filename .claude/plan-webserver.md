# Plan Claude-Web (Nico)

Zuständig für: **Webserver/API**, **Datenbank**, **Dashboard**, **KI-Modul**, **Deployment auf dem Raspberry Pi 5 inkl. WLAN-Hotspot**.
Schnittstelle zum ESP: `.claude/api-contract.md` – **zuerst lesen**. Regeln: `.claude/CLAUDE.md`.
Plan und Status bitte nur in dieser Datei pflegen (Abschnitt **Status** unten).

## Zielplattform

- Raspberry Pi 5, Raspberry Pi OS 64-bit (Bookworm/Trixie), Python 3.11+
- Alles läuft lokal auf dem Pi, **keine Cloud, keine CDNs** (Datenschutz + funktioniert ohne Internet)

## Empfohlener Stack

| Bereich | Wahl |
|---|---|
| API | FastAPI + Uvicorn |
| DB | SQLite (WAL-Modus), SQLAlchemy oder `sqlite3` |
| Dashboard | Statisches HTML/CSS/JS im Ordner `web/`, von FastAPI ausgeliefert; Chart.js **lokal** eingebunden |
| KI | Python-Paket `ai/` (numpy, optional scikit-learn), läuft im Server-Prozess |
| Reverse Proxy / TLS | Caddy mit `tls internal` (selbstsigniertes Zertifikat) |
| Dienste | systemd-Units in `deploy/` |
| Firewall | ufw: nur 22 (SSH), 80, 443 (+ 8000 während Phase 1) |

## Aufgaben

### 0. Netzwerk: WLAN-Hotspot auf dem Pi (`deploy/`)

- NetworkManager-Hotspot auf `wlan0`: SSID `SmartGarden`, WPA2, Pi-IP `10.42.0.1` (Standard bei `ipv4.method shared`)
  - z. B. `nmcli dev wifi hotspot ifname wlan0 ssid SmartGarden password <geheim>` + Autostart (`connection.autoconnect yes`)
- Internet für den Pi (Updates) über Ethernet, falls vorhanden
- WLAN-Passwort **nicht** committen; Florian bekommt es für `secrets.h` im ESP
- Möglichst früh einrichten – der ESP braucht das Netz für den ersten echten Test

### 1. API (`server/`)

- `POST /api/v1/readings` exakt nach Vertrag (Pydantic-Validierung, API-Key-Prüfung, Rate-Limit, Antwort mit `commands` + `config`)
- API-Keys **gehasht** speichern (z. B. SHA-256), Vergleich zeitkonstant
- Lese-Endpunkte fürs Dashboard, z. B.:
  - `GET /api/v1/devices/{id}/latest`
  - `GET /api/v1/devices/{id}/readings?from=&to=&bucket=` (Grafiken, optional aggregiert)
  - `GET /api/v1/devices/{id}/insights` → KI-Ergebnis (siehe 4.)
  - `GET /api/v1/alerts`, `POST /api/v1/alerts/{id}/ack`
  - `GET/PUT /api/v1/devices/{id}/config` (nur Admin; inkl. Tankgröße und Pumpen-Durchfluss)
  - `POST /api/v1/devices/{id}/commands` (nur Admin: manuell gießen, **Tank aufgefüllt**, identify)
- Live-Updates: Polling alle 5 s reicht; optional Server-Sent Events

### 2. Datenbank

Vorschlag Tabellen: `devices`, `readings`, `alerts`, `commands`, `device_config`, `users`, `access_log`.

- **Datenaufbewahrung:** Rohwerte 30 Tage, danach nur Stundenmittel (täglicher Job)
- Keine personenbezogenen Daten außer Benutzerkonten

### 3. Sicherheit

- Rollen **Admin** / **Leser**; Login mit Session-Cookie (HttpOnly, Secure, SameSite=Strict), Passwörter mit bcrypt/argon2
- Leser dürfen nur lesen; Konfiguration, Befehle, Benutzerverwaltung nur Admin
- **Zugriffsprotokoll** (Zeit, Benutzer/Gerät, IP, Methode, Pfad, Status) in `access_log` + Ansicht für Admin
- CSRF-Schutz für schreibende Dashboard-Requests, Security-Header (CSP ohne externe Quellen)
- **OWASP-Bedrohungsmodellierung** in `docs/server/security.md` (Florian liefert ESP-/Hardware-Teil zu)

### 4. KI-Modul (`ai/`)

Vorgeschlagene Schnittstelle (intern, darf Claude-Web frei ändern):

```python
from ai.garden_ai import analyze
result = analyze(readings: list[dict], config: dict) -> dict
```

- `readings` aufsteigend nach Zeit, max. 7 Tage, Felder aus dem Vertrag + `received_at`
- < 1 s auf dem Pi, keine Netzwerkzugriffe, keine Exceptions (zu wenig Daten → `null`)

Inhalte:

1. **Wasserbedarf-Prognose:** Trend der Bodenfeuchte (Regression, Pumpenereignisse herausgerechnet) → Stunden bis `moisture_min_pct`, korrigiert nach Temperatur/Licht
2. **Anomalie-Erkennung:** robuster Z-Score (Median/MAD), Sprungerkennung (z. B. plötzlicher Temperaturanstieg), „Sensor hängt“
3. **Pflanzenstress-Score** 0–100 mit Begründungen (`ok` / `warn` / `critical`)
4. **Tank-Prognose:** Wann ist der Tank leer (aus Verbrauchstrend)?
5. **Wasserersparnis/CO₂** gegenüber festem Gießplan (Nachhaltigkeit)
6. Ergebnisse mit `message_key`s für i18n; Unit-Tests mit synthetischen Daten

Beispiel-Rückgabe:

```json
{
  "model_version": "0.1.0",
  "water_forecast": {"hours_until_dry": 18.5, "recommend_water_now": false, "confidence": 0.7},
  "tank_forecast": {"days_until_empty": 4.2},
  "anomalies": [{"field": "air_temp_c", "received_at": "...", "value": 35.2, "score": 4.1, "message_key": "anomaly.temp_spike"}],
  "stress": {"score": 35, "level": "ok", "reasons": ["soil_dry"]},
  "water_saved_ml_estimate": 1200
}
```

### 5. Dashboard (`web/`)

- Kacheln: Bodenfeuchte, Licht, Temperatur, Luftfeuchte, **Tank (geschätzt, klar als Schätzung gekennzeichnet)**, Pumpe/Wasserverbrauch, KI-Prognose, Stress-Level
- Echtzeit-Grafiken (letzte Stunde / 24 h / 7 Tage)
- Verständliche Statusmeldungen (z. B. „Die Erde ist trocken – bald wird gegossen.“)
- **Mehrsprachig NL/DE/EN** über JSON-Sprachdateien; alle `message_key`s übersetzen
- **WCAG 2.1 AA:** Kontrast ≥ 4.5:1, Tastatur, sichtbarer Fokus, `aria-live` für Warnungen, nie nur Farbe, skalierbare Schrift, einfache Sprache
- Nachhaltigkeit: gesparte Wassermenge, CO₂-Schätzung
- Admin: Grenzwerte, Tankgröße, Durchfluss, manuell gießen, „Tank aufgefüllt“, Benutzer, Zugriffsprotokoll

### 6. Deployment (`deploy/`)

- Installationsanleitung/Skript (Hotspot, venv, systemd, Caddy, ufw)
- Tägliches Backup der SQLite-DB (`sqlite3 .backup`)
- Optional Monitoring (`/healthz`, später Grafana/Prometheus)

## Testen ohne Hardware

`python tools/fake_esp.py --url http://localhost:8000 --key <key> --interval 2` (liefert Claude-ESP).

## Reihenfolge

| Phase | Inhalt |
|---|---|
| 1 | Hotspot auf dem Pi, FastAPI-Grundgerüst, `POST /readings` + DB, einfaches Dashboard |
| 2 | Grafiken, Warnungen, Login + Rollen, i18n, KI-Stub |
| 3 | KI v1, Admin-Funktionen (Config, Befehle, Tank), HTTPS via Caddy, Zugriffsprotokoll |
| 4 | Deployment finalisieren, Tests, Sicherheitsanalyse, Infrastrukturdiagramm |

## Status

_(von Claude-Web gepflegt)_

- [ ] Phase 1
