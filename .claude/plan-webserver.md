# Plan Claude-Web (Nico)

Zuständig für: **Backend/API**, **Datenbank**, **Dashboard**, **KI-Modul**, **Deployment auf dem Raspberry Pi 5 inkl. WLAN-Hotspot**.
Schnittstelle zum ESP: `.claude/api-contract.md` – **zuerst lesen**. Regeln: `.claude/CLAUDE.md`.
Plan und Status bitte nur in dieser Datei pflegen (Abschnitt **Status** unten).

## Festgelegter Tech-Stack (vom Team entschieden)

| Bereich | Technik |
|---|---|
| Backend/API | **Python** – FastAPI + Uvicorn, Pydantic |
| Datenbank | SQLite (WAL-Modus), SQLAlchemy |
| KI | **Python** – scikit-learn, numpy/pandas; Chatbot über **Ollama** lokal auf dem Pi |
| Frontend | **React + Tailwind CSS** (Vite, TypeScript empfohlen) |
| Diagramme | Recharts |
| Mehrsprachigkeit | react-i18next (NL/DE/EN) |
| Reverse Proxy / TLS | Caddy mit `tls internal` |
| Dienste | systemd-Units in `deploy/` |
| Firewall | ufw: 22, 80, 443 (+ 8000 während Phase 1) |

Zielplattform: Raspberry Pi 5, **8 GB RAM**, Raspberry Pi OS 64-bit, Python 3.11+, Node.js nur zum Bauen.
Alles läuft lokal, **keine Cloud, keine CDNs**, keine externen Fonts (Datenschutz + funktioniert ohne Internet).

## Ordnerstruktur

```
server/        FastAPI-App (app/main.py, routers/, models.py, db.py, auth.py, alerts.py)
ai/            KI-Paket (garden_ai.py, models/, train.py, chatbot.py, tests/)
web/           React-Projekt (Vite + Tailwind); `npm run build` → web/dist/
deploy/        Hotspot-Setup, systemd-Units, Caddyfile, Install-Skript
docs/server/   Sicherheitsanalyse, Infrastrukturdiagramm, API-Doku
```

- Entwicklung: FastAPI auf dem Laptop (`uvicorn --reload`), React mit `npm run dev` (Vite-Proxy auf `/api`). Testdaten mit `tools/fake_esp.py`.
- Produktion: `web/dist/` wird von Caddy (oder FastAPI) als statische Seite ausgeliefert, `/api` geht an FastAPI. Bauen auf dem Laptop oder direkt auf dem Pi.

## Aufgaben

### 0. Netzwerk: WLAN-Hotspot auf dem Pi (`deploy/`) – zuerst!

- NetworkManager-Hotspot auf `wlan0`: SSID `SmartGarden`, WPA2, Pi-IP `10.42.0.1`
  - z. B. `nmcli dev wifi hotspot ifname wlan0 ssid SmartGarden password <geheim>` + `connection.autoconnect yes`
- Internet für den Pi (Updates, Ollama-Modell laden) über Ethernet
- WLAN-Passwort **nicht** committen; Florian bekommt es für `secrets.h` im ESP

### 1. API (`server/`)

- `POST /api/v1/readings` exakt nach Vertrag (Pydantic, API-Key-Prüfung, Rate-Limit, Antwort mit `commands` + `config`)
- API-Keys **gehasht** speichern, Vergleich zeitkonstant; Gerät + Key per CLI-Befehl anlegen
- Endpunkte fürs Dashboard, z. B.:
  - `GET /api/v1/devices/{id}/latest`
  - `GET /api/v1/devices/{id}/readings?from=&to=&bucket=`
  - `GET /api/v1/devices/{id}/insights` (KI-Ergebnis)
  - `POST /api/v1/chat` (Chatbot, siehe 4.)
  - `GET /api/v1/alerts`, `POST /api/v1/alerts/{id}/ack`
  - `GET/PUT /api/v1/devices/{id}/config` (Admin; inkl. Tankgröße, Durchfluss)
  - `POST /api/v1/devices/{id}/commands` (Admin: gießen, **Tank aufgefüllt**, identify)
  - `POST /api/v1/auth/login`, `/logout`, `GET /api/v1/auth/me`
- OpenAPI-Doku von FastAPI ist gleichzeitig die API-Dokumentation

### 2. Datenbank

Tabellen: `devices`, `readings`, `alerts`, `commands`, `device_config`, `users`, `access_log`, `insights_cache`.

- **Datenaufbewahrung:** Rohwerte 30 Tage, danach Stundenmittel (täglicher Job)
- Keine personenbezogenen Daten außer Benutzerkonten; Chat-Verläufe **nicht** dauerhaft speichern

### 3. Sicherheit

- Rollen **Admin** / **Leser**; Session-Cookie (HttpOnly, Secure, SameSite=Strict), Passwörter mit argon2/bcrypt
- Leser nur lesen (+ Chatbot); Config, Befehle, Benutzer nur Admin
- **Zugriffsprotokoll** (Zeit, Benutzer/Gerät, IP, Methode, Pfad, Status) + Admin-Ansicht
- CSRF-Schutz, Security-Header (CSP ohne externe Quellen), Rate-Limit auf Login und Chat
- Chatbot: Prompt-Injection bedenken – Bot hat **nur Lesezugriff** auf Messwerte, kann keine Befehle auslösen
- **OWASP-Bedrohungsmodell** in `docs/server/security.md` (Florian liefert ESP-/Hardware-Teil)

### 4. KI (`ai/`, komplett Python)

Stufe B – Modelle (Pflicht):

1. **Wasserbedarf-Prognose:** scikit-learn-Regressionsmodell (z. B. GradientBoosting/RandomForest) – Eingaben: aktuelle Feuchte, Trend, Temperatur, Luftfeuchte, Licht, Tageszeit → Stunden bis `moisture_min_pct`. Training auf simulierten Verläufen (`train.py`), später mit echten Daten nachtrainieren. Modell als Datei in `ai/models/`.
2. **Anomalie-Erkennung:** IsolationForest + einfache Regeln (Sprung, „Sensor hängt“, plötzlicher Temperaturanstieg)
3. **Pflanzenstress-Score** 0–100 mit Begründungen (`ok` / `warn` / `critical`)
4. **Tank-Prognose:** Tage bis Tank leer (aus Verbrauchstrend)
5. **Wasserersparnis/CO₂** gegenüber festem Gießplan

Stufe C – Pflanzen-Chatbot (wenn Zeit):

- **Ollama** auf dem Pi, kleines Modell (z. B. `llama3.2:3b` oder `qwen2.5:3b`, ~2–3 GB RAM)
- Server baut den Prompt aus aktuellen Messwerten + KI-Ergebnis + kurzem Pflanzenwissen (Tomate, Erdbeere, Kräuter, Kapuzinerkresse)
- Antwort in der Sprache der Oberfläche (NL/DE/EN), Streaming ans Frontend
- Läuft komplett offline → Datenschutz-Argument für die Präsentation

Schnittstelle (Vorschlag):

```python
from ai.garden_ai import analyze
result = analyze(readings: list[dict], config: dict) -> dict   # < 1 s, keine Exceptions
```

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

### 5. Dashboard (`web/`, React + Tailwind)

- Kacheln: Bodenfeuchte, Licht, Temperatur, Luftfeuchte, **Tank (als Schätzung gekennzeichnet)**, Pumpe/Wasserverbrauch, KI-Prognose, Stress-Level
- Echtzeit-Diagramme (1 h / 24 h / 7 Tage), Polling alle 5 s
- Verständliche Statusmeldungen („Die Erde ist trocken – bald wird gegossen.“)
- Chatbot-Fenster (Stufe C)
- **NL/DE/EN** mit react-i18next; alle `message_key`s übersetzen; Sprache umschaltbar
- **WCAG 2.1 AA:** Kontrast ≥ 4.5:1, Tastatur, sichtbarer Fokus, `aria-live` für Warnungen, nie nur Farbe, skalierbare Schrift, einfache Sprache, Hell/Dunkel-Modus
- Nachhaltigkeit: gesparte Wassermenge, CO₂-Schätzung
- Admin: Grenzwerte, Tankgröße, Durchfluss, gießen, „Tank aufgefüllt“, Benutzer, Zugriffsprotokoll

### 6. Deployment (`deploy/`)

- Install-Skript: Hotspot, Python-venv, systemd, Caddy, ufw, Ollama
- Tägliches SQLite-Backup (`sqlite3 .backup`)
- `/healthz`; optional Monitoring (Prometheus/Grafana)

## Reihenfolge

| Phase | Inhalt |
|---|---|
| 1 | Hotspot, FastAPI-Grundgerüst, `POST /readings` + DB, React-Grundgerüst mit aktuellen Werten |
| 2 | Diagramme, Warnungen, Login + Rollen, i18n, KI-Stub |
| 3 | KI Stufe B, Admin-Funktionen, HTTPS via Caddy, Zugriffsprotokoll |
| 4 | Chatbot (Stufe C), Deployment finalisieren, Tests, Sicherheitsanalyse, Infrastrukturdiagramm |

## Status

_(von Claude-Web gepflegt)_

- [ ] Phase 1
