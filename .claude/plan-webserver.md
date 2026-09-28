# Plan Claude-Web (Nico)

Zuständig für: **Backend/API**, **Datenbank**, **Dashboard**, **KI-Modul**, **Deployment auf dem Raspberry Pi 5 inkl. WLAN-Hotspot**.
Schnittstelle zum ESP: `.claude/api-contract.md` (v1.2) – **zuerst lesen**. Regeln: `.claude/CLAUDE.md`.
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
server/        FastAPI-App (app/main.py, routers/, models.py, db.py, auth.py, alerts.py, demo.py)
ai/            KI-Paket (garden_ai.py, models/, train.py, chatbot.py, tests/)
web/           React-Projekt (Vite + Tailwind); `npm run build` → web/dist/
deploy/        Hotspot-Setup, systemd-Units, Caddyfile, Install-Skript
docs/server/   Sicherheitsanalyse, Infrastrukturdiagramm, API-Doku
```

- Entwicklung: FastAPI auf dem Laptop (`uvicorn --reload`), React mit `npm run dev` (Vite-Proxy auf `/api`). Testdaten mit `tools/fake_esp.py`.
- Produktion: `web/dist/` wird von Caddy (oder FastAPI) ausgeliefert, `/api` geht an FastAPI.

## Aufgaben

### 0. Netzwerk: WLAN-Hotspot auf dem Pi (`deploy/`) – zuerst!

- NetworkManager-Hotspot auf `wlan0`: SSID `SmartGarden`, WPA2, Pi-IP `10.42.0.1`
  - z. B. `nmcli dev wifi hotspot ifname wlan0 ssid SmartGarden password <geheim>` + `connection.autoconnect yes`
- Internet für den Pi (Updates, Ollama-Modell laden) über Ethernet
- WLAN-Passwort **nicht** committen; Florian bekommt es für `secrets.h` im ESP

### 1. API (`server/`)

- `POST /api/v1/readings` exakt nach Vertrag (Pydantic, API-Key-Prüfung, Rate-Limit, Antwort mit `commands`, `config`, `demo`)
- API-Keys **gehasht** speichern, Vergleich zeitkonstant; Gerät + Key per CLI-Befehl anlegen
- Endpunkte fürs Dashboard, z. B.:
  - `GET /api/v1/devices/{id}/latest`
  - `GET /api/v1/devices/{id}/readings?from=&to=&bucket=`
  - `GET /api/v1/devices/{id}/insights` (KI-Ergebnis)
  - `POST /api/v1/chat` (Chatbot, siehe 4.)
  - `GET /api/v1/alerts`, `POST /api/v1/alerts/{id}/ack`
  - `GET/PUT /api/v1/devices/{id}/config` (Admin; inkl. Tankgröße, Durchfluss)
  - `POST /api/v1/devices/{id}/commands` (Admin: gießen, **Tank aufgefüllt**, identify)
  - `GET/PUT/DELETE /api/v1/devices/{id}/demo` (Admin: Demo-Modus, siehe 5.)
  - `POST /api/v1/auth/login`, `/logout`, `GET /api/v1/auth/me`
- OpenAPI-Doku von FastAPI ist gleichzeitig die API-Dokumentation

### 2. Datenbank

Tabellen: `devices`, `readings`, `alerts`, `commands`, `device_config`, `demo_state`, `users`, `access_log`, `insights_cache`.

- **Alle** Felder aus dem Vertrag speichern (auch Rohwerte, RSSI, Uptime, Fehler, `demo_overrides`)
- **Datenaufbewahrung:** Rohwerte 30 Tage, danach Stundenmittel (täglicher Job)
- Keine personenbezogenen Daten außer Benutzerkonten; Chat-Verläufe **nicht** dauerhaft speichern

### 3. Sicherheit

- Rollen **Admin** / **Leser**; Session-Cookie (HttpOnly, Secure, SameSite=Strict), Passwörter mit argon2/bcrypt
- Leser nur lesen (+ Chatbot); Config, Befehle, Demo-Modus, Benutzer nur Admin
- **Zugriffsprotokoll** (Zeit, Benutzer/Gerät, IP, Methode, Pfad, Status) + Admin-Ansicht
- CSRF-Schutz, Security-Header (CSP ohne externe Quellen), Rate-Limit auf Login und Chat
- Chatbot: Prompt-Injection bedenken – Bot hat **nur Lesezugriff** auf Messwerte, kann keine Befehle auslösen
- **OWASP-Bedrohungsmodell** in `docs/server/security.md` (Florian liefert ESP-/Hardware-Teil)

### 4. KI (`ai/`, komplett Python)

Stufe B – Modelle (Pflicht):

1. **Wasserbedarf-Prognose:** scikit-learn-Regressionsmodell (z. B. GradientBoosting/RandomForest) – Eingaben: Feuchte, Trend, Temperatur, Luftfeuchte, Licht, Tageszeit → Stunden bis `moisture_min_pct`. Training auf simulierten Verläufen (`train.py`), später mit echten Daten. Modell als Datei in `ai/models/`.
2. **Anomalie-Erkennung:** IsolationForest + Regeln (Sprung, „Sensor hängt“, plötzlicher Temperaturanstieg)
3. **Pflanzenstress-Score** 0–100 mit Begründungen (`ok` / `warn` / `critical`)
4. **Tank-Prognose:** Tage bis Tank leer
5. **Wasserersparnis/CO₂** gegenüber festem Gießplan
6. Demo-Werte (`demo_overrides` nicht leer) **nicht** fürs Training verwenden, aber normal analysieren (damit die KI in der Demo reagiert)

Stufe C – Pflanzen-Chatbot (wenn Zeit):

- **Ollama** auf dem Pi, kleines Modell (z. B. `llama3.2:3b` oder `qwen2.5:3b`)
- Prompt aus aktuellen Messwerten + KI-Ergebnis + Pflanzenwissen (Tomate, Erdbeere, Kräuter, Kapuzinerkresse)
- Antwort in der Sprache der Oberfläche (NL/DE/EN), Streaming ans Frontend; komplett offline

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

### 5. Demo-Modus (wichtig für die Präsentation!)

Vertrag Abschnitt 4. Admin kann im Dashboard Sensorwerte überschreiben; der Server schickt sie im Feld `demo` an den ESP, die echte Hardware reagiert.

Dashboard-Bereich „Demo“ (nur Admin):

- **Szenario-Buttons** (ein Klick):

| Szenario | Overrides / Aktion | Erwartete Reaktion |
|---|---|---|
| Trockene Erde | `soil_moisture_pct: 12` | LED-Bar rot, Warnung, ESP gießt automatisch, KI empfiehlt Gießen |
| Hitzewelle | `air_temp_c: 38`, `air_humidity_pct: 25` | Stress „critical“, Anomalie „Temperatursprung“, Warnung |
| Tank fast leer | `water_level_pct: 4` | Warnung, Summer, Auto-Bewässerung gestoppt |
| Sensorausfall | `force_errors: ["dht_read_failed"]` | Fehler-Warnung, Kachel zeigt „Sensor nicht erreichbar“ |
| Nacht | `light_pct: 2` | Licht-Kachel, KI berücksichtigt Dunkelheit |
| Alles zurücksetzen | `demo: null` | Sofort echte Werte |

- Zusätzlich **Schieberegler** pro Messwert (Bodenfeuchte, Licht, Temperatur, Luftfeuchte, Tank) für freie Werte
- Laufzeit wählbar (Standard 120 s, max. 600 s), Countdown anzeigen
- Solange aktiv: gut sichtbarer **„DEMO-MODUS“-Banner** für alle Nutzer; überschriebene Werte in Kacheln und Diagrammen markieren
- Optional „Demo-Cooldown“: Knopf, der `pump_cooldown_s` vorübergehend auf 30 s setzt, damit mehrere Bewässerungen hintereinander gezeigt werden können
- Jede Aktion ins Zugriffsprotokoll

### 6. Dashboard (`web/`, React + Tailwind)

**Alle Sensordaten** werden angezeigt:

| Kachel | Felder |
|---|---|
| Bodenfeuchte | `soil_moisture_pct` (+ Rohwert in Detailansicht), Farbskala Grün→Rot wie die LED-Bar |
| Licht | `light_pct` (+ Rohwert) |
| Temperatur | `air_temp_c` |
| Luftfeuchte | `air_humidity_pct` |
| Wassertank | `water_level_pct`, `tank_remaining_ml` – **als Schätzung gekennzeichnet** |
| Pumpe | `pump_running`, Laufzeit heute, Wasserverbrauch, letzte Auto-Bewässerung |
| Gerät | Online/Offline (letzter Kontakt), `rssi_dbm`, `uptime_s`, `fw_version`, `errors` |
| KI | Wasserprognose, Stress-Level, Anomalien, Tank-Prognose |

- Echtzeit-Diagramme für alle Messwerte (1 h / 24 h / 7 Tage), Polling alle 5 s
- Verständliche Statusmeldungen („Die Erde ist trocken – bald wird gegossen.“) + Benachrichtigungsliste
- Chatbot-Fenster (Stufe C)
- **NL/DE/EN** mit react-i18next; alle `message_key`s übersetzen; Sprache umschaltbar
- **WCAG 2.1 AA:** Kontrast ≥ 4.5:1, Tastatur, sichtbarer Fokus, `aria-live` für Warnungen, **nie nur Farbe** (Grün→Rot immer mit Zahl/Text/Icon), skalierbare Schrift, einfache Sprache, Hell/Dunkel-Modus
- Nachhaltigkeit: gesparte Wassermenge, CO₂-Schätzung
- Admin: Grenzwerte, Tankgröße, Durchfluss, gießen, „Tank aufgefüllt“, Demo-Modus, Benutzer, Zugriffsprotokoll

### 7. Deployment (`deploy/`)

- Install-Skript: Hotspot, Python-venv, systemd, Caddy, ufw, Ollama
- Tägliches SQLite-Backup (`sqlite3 .backup`)
- `/healthz`; optional Monitoring (Prometheus/Grafana)

## Reihenfolge

| Phase | Inhalt |
|---|---|
| 1 | Hotspot, FastAPI-Grundgerüst, `POST /readings` + DB, React-Grundgerüst mit allen aktuellen Werten |
| 2 | Diagramme, Warnungen, Login + Rollen, i18n, **Demo-Modus**, KI-Stub |
| 3 | KI Stufe B, Admin-Funktionen, HTTPS via Caddy, Zugriffsprotokoll |
| 4 | Chatbot (Stufe C), Deployment finalisieren, Tests, Sicherheitsanalyse, Infrastrukturdiagramm |

## Status

_(von Claude-Web gepflegt)_

### Phase 1 (Branch `feature/server-phase1-grundgeruest`)

- [x] `deploy/setup_hotspot.sh`: NetworkManager-Hotspot "SmartGarden" auf `wlan0`, Pi-IP `10.42.0.1`,
      autoconnect, ufw-Regeln (22/80/443/8000). Noch **nicht auf echtem Pi getestet** (kein Pi verfuegbar) -
      Skript ist idempotent und prueft `nmcli`-Verfuegbarkeit; Test folgt sobald Hardware da ist.
      `deploy/smart-garden-api.service` (systemd-Unit) + `deploy/README.md` (Setup-Anleitung) ebenfalls angelegt.
- [x] `server/`: FastAPI-Grundgerüst (SQLAlchemy/SQLite, WAL-Modus)
  - `POST /api/v1/readings` exakt nach Vertrag v1.2: Pydantic-Validierung, `X-API-Key`-Pruefung (argon2,
    zeitkonstant), Rate-Limit 1 Request/2s pro Geraet (429), Antwort mit `commands`/`config`/`demo` (demo
    aktuell immer `null`, folgt Phase 2)
  - `GET /api/v1/devices/{id}/latest` fuers Dashboard (noch ohne Auth - Login/Rollen kommen Phase 2)
  - `GET /healthz`
  - `python -m app.cli create-device <id>` zum Anlegen von Geraet + API-Key (Key wird einmalig im Klartext
    ausgegeben, in DB nur Hash)
  - Getestet lokal mit `tools/fake_esp.py` (von Claude-ESP, Branch `feature/esp-fake-esp`, noch nicht in
    `main`): 200 bei gueltigem Request, 401 bei falschem/fehlendem Key und unbekanntem Geraet, 429 bei zu
    schnellen Requests, `GET .../latest` liefert korrekt zurueck was `fake_esp.py` gesendet hat
  - Tabellen `devices`, `readings`, `device_config`, `pending_commands` (fuer `alerts`, `users`,
    `access_log`, `demo_state`, `insights_cache` siehe Phase 2/3)
- [x] `web/`: React-Grundgerüst (Vite + TypeScript + Tailwind v4, react-i18next noch nicht eingebunden -
      folgt mit Mehrsprachigkeit in Phase 2)
  - `Dashboard`-Komponente pollt `GET /api/v1/devices/{id}/latest` alle 5s (Vite-Dev-Proxy `/api` ->
    `localhost:8000`) und zeigt alle aktuellen Sensordaten (Bodenfeuchte inkl. Rohwert, Licht, Temperatur,
    Luftfeuchte, Tank inkl. Schaetzungs-Hinweis, Pumpe, Geraetestatus/RSSI/Uptime, Fehler, DEMO-Banner falls
    `demo_overrides` gesetzt)
  - `tsc -b` laeuft fehlerfrei durch; End-to-End gegen laufenden FastAPI-Server + `fake_esp.py` getestet
  - Geraete-ID (`esp32-kuebel-01`) ist Phase 1 noch fest verdrahtet - Geraeteauswahl folgt Phase 2
  - Farbskala Bodenfeuchte ist vorbereitet (gruen/gelb/rot je nach `%`), aber **nicht** die einzige
    Information (Zahl+Text immer sichtbar, WCAG-Vorgabe)

### Phase 2 – Teil 1: Diagramme (Branch `feature/web-diagramme`)

- [x] `GET /api/v1/devices/{id}/readings?from=&to=&bucket=`: Mittelwerte je Zeit-Bucket (automatisch ~120 Punkte,
      "runde" Bucket-Groessen 15 s ... 6 h), Summe Pumpenlaufzeit/Wasser-ml, Anzahl Auto-Bewaesserungen, Demo-Flag,
      Fehlercodes; max. 31 Tage. `GET /api/v1/devices/{id}/config` (nur lesen) fuer Grenzwert-Linien.
- [x] Zeitstempel jetzt mit `Z` (UTC) ausgeliefert - vorher zeigte der Browser UTC als Ortszeit an (2 h daneben)
- [x] Dashboard "Verlauf": Zeitraum 1 h / 24 h / 7 Tage, Kennzahlen (Wasserverbrauch, Anzahl Giessen,
      Ø Bodenfeuchte), 6 Diagramme: Bodenfeuchte (mit Giessschwelle/Ziel + Markern fuer Auto-Bewaesserung),
      Tank (mit Warnschwelle), Wasserverbrauch pro 5 min/Stunde/Tag, Temperatur, Luftfeuchte, Licht.
      Demo-Zeitraeume grau-violett hinterlegt, Sensorausfall = Luecke in der Linie, Tooltip mit Fehlertexten,
      jede Grafik auch als Tabelle (WCAG), hell/dunkel mit geprueften Farben
- [x] `python -m app.dev_seed <id> --hours 48`: simulierter Verlauf fuer die Entwicklung (nicht auf dem Pi!)
- [x] Gerendert und geprueft (Desktop hell/dunkel, Mobil 390 px, Hover, 7-Tage-Ansicht)
- [x] `pydantic` 2.9.2 -> 2.13.5: alte Version hatte keine fertigen Pakete fuer Python 3.14 (Nicos Mac) und
      liess sich dort nicht bauen. Geprueft: fertige Pakete fuer macOS arm64/Py3.14 und Pi (aarch64, Py3.11/3.13)

### Phase 2 – Teil 2: Demo-Modus (Branch `feature/web-demo-modus`)

- [x] `GET/PUT/DELETE /api/v1/devices/{id}/demo`: Overrides (nur Vertragsfelder, Wertebereiche geprueft),
      erzwungene Fehler (`dht_read_failed`, `soil_out_of_range`, `light_read_failed`), Dauer 10–600 s, Szenario-Name.
      Tabelle `demo_state`; laeuft serverseitig ab. `POST /readings` schickt `demo` jetzt echt an den ESP
      (`expires_in_s` = Restzeit).
- [x] `GET/POST /api/v1/devices/{id}/commands`: gießen (auf `max_pump_s_per_run` begrenzt), Tank aufgefuellt,
      identify, Summer - genau einmal zugestellt (Tabelle `pending_commands`).
- [x] Dashboard "Demo-Steuerung": 5 Szenario-Buttons aus Abschnitt 5, eigene Werte per Schieberegler,
      Sensorausfall-Checkboxen, Dauer 1/2/5/10 min, Countdown, "Demo beenden", Geraetebefehle,
      Anzeige ob das Geraet die Demo uebernommen hat; Banner fuer alle oben (sticky); DEMO-Badge an Kacheln;
      Steuerung ein-/ausblendbar (fuer die Beamer-Ansicht).
- [x] Getestet mit `fake_esp.py` (auch unter Python 3.14): Szenarien kommen an, ESP giesst bei "Trockene Erde",
      Befehle einmal zugestellt, Ablauf/Beenden, 422 fuer ungueltige Eingaben; Browser-Klicktest per Playwright.
- [ ] **Offen: nur Admin** - Login/Rollen fehlen noch, bis dahin kann jeder im Hotspot-WLAN Demo/Befehle ausloesen.
      Aenderungen stehen vorerst im Server-Log (`smart_garden.control`), echtes Zugriffsprotokoll folgt.
- [ ] Optional aus dem Plan noch nicht gebaut: "Demo-Cooldown" (pump_cooldown_s kurz auf 30 s)

### Phase 2 – Teil 3: Handy (Branch `feature/web-mobil`)

- [x] `npm run dev:mobil` (= `vite --host`): Dashboard im WLAN erreichbar, `/api` wird weiter vom Mac-Proxy bedient
- [x] FastAPI liefert `web/dist/` unter `/` aus, falls gebaut (`SMART_GARDEN_WEB_DIST` ueberschreibbar) -
      auf dem Pi reicht damit ein Dienst; Handy im Hotspot: `http://10.42.0.1:8000`
- [x] Web-App-Manifest + Icons (192/512/maskable/apple-touch), theme-color hell/dunkel, viewport-fit=cover
- [x] Handy-Layout: 2 Spalten Kacheln + Szenarien, Touch-Ziele >= 44 px, safe-area (Notch/Home-Leiste),
      Fokusrahmen nach Antippen von Diagrammen weg (Tastatur-Fokus bleibt), Demo-Beschriftung in der Legende
- [x] Geprueft: iPhone-13-Emulation hell/dunkel, Tipp auf Szenario + Diagramm, kein horizontales Scrollen,
      `dev:mobil` per Netzwerk-IP inkl. `/api`, FastAPI liefert Seite/Manifest/Icons + API parallel
- [ ] Kein Service Worker/Offline-Modus (braucht HTTPS - kommt ggf. mit Caddy in Phase 3)

### Als Naechstes

- Login + Rollen (Admin/Leser) und damit Demo/Befehle absichern, Zugriffsprotokoll
- Warnungen/Alerts, i18n (NL/DE/EN), KI-Stub
- Mit echtem ESP testen (Florian): Demo-Szenarien, "Tank aufgefuellt" nach "Tank fast leer"
- Hotspot-Skript auf echter Pi-Hardware verifizieren, sobald verfuegbar

### Blocker

- Kein Zugriff auf echte Pi-Hardware in dieser Umgebung -> `deploy/setup_hotspot.sh` ist ungetestet.
