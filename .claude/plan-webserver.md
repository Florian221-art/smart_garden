# Plan Claude-Web (Nico)

Zuständig für: **Webserver/API**, **Datenbank**, **Dashboard**, **Deployment auf dem Raspberry Pi 5**.
Schnittstelle zum ESP und zum KI-Modul: `.claude/api-contract.md` – **zuerst lesen**. Regeln: `.claude/CLAUDE.md`.
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
| Reverse Proxy / TLS | Caddy mit `tls internal` (selbstsigniertes Zertifikat) |
| Dienste | systemd-Units in `deploy/` |
| Firewall | ufw: nur 22 (SSH), 80, 443 |

## Aufgaben

### 1. API (`server/`)

- `POST /api/v1/readings` exakt nach Vertrag (Validierung mit Pydantic, API-Key-Prüfung, Rate-Limit, Antwort mit `commands` + `config`)
- API-Keys **gehasht** speichern (z. B. SHA-256), Vergleich zeitkonstant
- Lese-Endpunkte fürs Dashboard, z. B.:
  - `GET /api/v1/devices/{id}/latest`
  - `GET /api/v1/devices/{id}/readings?from=&to=&bucket=` (für Grafiken, optional aggregiert)
  - `GET /api/v1/devices/{id}/insights` → ruft `ai.garden_ai.analyze(...)` auf
  - `GET /api/v1/alerts`, `POST /api/v1/alerts/{id}/ack`
  - `GET/PUT /api/v1/devices/{id}/config` (nur Admin)
  - `POST /api/v1/devices/{id}/commands` (nur Admin, z. B. manuell gießen)
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
- Grundlage für die **OWASP-Bedrohungsmodellierung** in `docs/server/security.md`

### 4. Dashboard (`web/`)

- Kacheln: Bodenfeuchte, Licht, Temperatur, Luftfeuchte, Pumpe/Wasserverbrauch, KI-Prognose, Stress-Level
- Echtzeit-Grafiken (letzte Stunde / 24 h / 7 Tage)
- Statusmeldungen/Warnungen, verständlich formuliert (z. B. „Die Erde ist trocken – bald wird gegossen.“)
- **Mehrsprachig NL/DE/EN** über JSON-Sprachdateien; `message_key`s aus Server und KI übersetzen
- **WCAG 2.1 AA:** Kontrast ≥ 4.5:1, Tastaturbedienung, sichtbarer Fokus, `aria-live` für Warnungen, Informationen nie nur über Farbe, skalierbare Schrift, einfache Sprache
- Nachhaltigkeit: Anzeige gesparter Wassermenge und CO₂-Schätzung
- Admin-Bereich: Grenzwerte, manuelles Gießen, Benutzer, Zugriffsprotokoll

### 5. Deployment (`deploy/`)

- Installationsanleitung/Skript für den Pi (venv, systemd, Caddy, ufw)
- Backup der SQLite-DB (täglich, `sqlite3 .backup`)
- Optional: Monitoring (z. B. `/healthz`-Endpunkt, später Grafana/Prometheus)

## Testen ohne Hardware

`python tools/fake_esp.py --url http://localhost:8000 --key <key> --interval 2` (liefert Claude-ESP). Bis das KI-Modul fertig ist, liefert `ai.garden_ai.analyze` einen Stub mit gültiger Struktur.

## Reihenfolge

| Phase | Inhalt |
|---|---|
| 1 | FastAPI-Grundgerüst, `POST /readings` + DB, einfaches Dashboard mit aktuellen Werten |
| 2 | Grafiken, Warnungen, Login + Rollen, i18n |
| 3 | KI-Integration, Admin-Funktionen, HTTPS via Caddy, Zugriffsprotokoll |
| 4 | Deployment auf dem Pi, Tests, Sicherheitsanalyse, Infrastrukturdiagramm |

## Status

_(von Claude-Web gepflegt)_

- [ ] Phase 1
