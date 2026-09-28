# Claude-Web – Fortschritt & Architektur (Stand: siehe Datum unten)

Diese Datei dokumentiert für Menschen (Nico, Florian), was im Bereich
Backend/Dashboard/Deployment bisher gebaut und getestet wurde. Der laufend
gepflegte Kurzstatus (fertig/als Nächstes/Blocker) steht wie vereinbart in
`.claude/plan-webserver.md`; hier steht die ausführlichere Erklärung dazu.

Letzte Aktualisierung: 2026-09-28

## Was schon läuft (Phase 1)

### 1. WLAN-Hotspot (`deploy/`)

- `deploy/setup_hotspot.sh`: Richtet per `nmcli` einen NetworkManager-Hotspot
  "SmartGarden" (WPA2) auf `wlan0` ein, Pi-IP `10.42.0.1`, startet automatisch
  beim Boot, setzt ufw-Regeln für 22/80/443/8000.
- `deploy/smart-garden-api.service`: systemd-Unit, die den FastAPI-Server
  über uvicorn als Dienst `smartgarden` laufen lässt.
- `deploy/README.md`: Schritt-für-Schritt-Anleitung fürs erste Deployment.
- **Ungetestet auf echter Pi-Hardware** – es stand in dieser Umgebung kein
  Pi zur Verfügung. Bitte auf dem Pi gegenprüfen, sobald er da ist.

### 2. Backend (`server/`)

Tech: FastAPI + Pydantic + SQLAlchemy (SQLite, WAL-Modus), argon2 für
API-Key-Hashing.

- `POST /api/v1/readings` – exakt nach `.claude/api-contract.md` v1.2:
  - Pydantic validiert Body-Felder inkl. Wertebereiche (z. B.
    `soil_moisture_pct` 0–100 oder `null`)
  - `X-API-Key`-Header wird gegen den gehashten Key des Geräts geprüft
    (argon2, dadurch zeitkonstant) → 401 bei falsch/fehlend/unbekanntem
    Gerät
  - Rate-Limit: max. 1 Request/2 s pro Gerät (In-Memory) → 429
  - Antwort enthält `commands` (aktuell immer leer/0/false – Admin-Endpunkt
    zum Setzen kommt in Phase 3), `config` (Default-Werte aus dem Vertrag)
    und `demo` (aktuell immer `null` – Demo-Modus-Endpunkte kommen Phase 2)
  - `received_at` wird serverseitig gesetzt (UTC), wie im Vertrag gefordert
- `GET /api/v1/devices/{id}/latest` – letzter Messwert eines Geräts, für
  das Dashboard. Noch **ohne Authentifizierung** (Login/Rollen sind
  Phase 2) – bewusst so, damit das Dashboard schon funktioniert.
- `GET /healthz` – einfacher Liveness-Check.
- `python -m app.cli create-device <id> [--label ...]` – legt ein Gerät an
  und gibt den API-Key **einmalig im Klartext** aus (danach nur Hash in
  der DB).
- Tabellen: `devices`, `readings` (alle Vertragsfelder inkl. Rohwerte,
  RSSI, Uptime, Fehler, `demo_overrides`), `device_config` (Config pro
  Gerät, aktuell nur Defaults), `pending_commands` (Ein-mal-Befehle,
  aktuell nur Struktur, noch kein Admin-Endpunkt zum Setzen).

### 3. Dashboard (`web/`)

Tech: React 19 + Vite + TypeScript + Tailwind v4.

- `Dashboard`-Komponente pollt `GET /api/v1/devices/{id}/latest` alle 5 s
  (Vite-Dev-Proxy `/api` → `localhost:8000`) und zeigt alle aktuellen
  Sensordaten: Bodenfeuchte (inkl. Rohwert), Licht (inkl. Rohwert),
  Temperatur, Luftfeuchte, Tank (mit Hinweis "Schätzung"), Pumpe,
  Gerätestatus (RSSI/Uptime/Firmware-Version), Fehler.
- Fehlercodes werden in verständlichen deutschen Text übersetzt (siehe
  Abschnitt "Neu seit dem ESP-Merge" unten) statt nur den rohen Code
  anzuzeigen.
- Farbskala Bodenfeuchte Rot→Grün wie die LED-Bar auf dem ESP, aber Farbe
  ist **nie die einzige Information** (Zahl/Text steht immer dabei) –
  WCAG-Vorgabe aus dem Plan.
- Banner für DEMO-Modus ist vorbereitet (`demo_overrides`-Feld), zeigt
  sich sobald der Server im Demo-Modus Werte überschreibt (Phase 2).
- Geräte-ID ist noch fest auf `esp32-kuebel-01` verdrahtet –
  Geräteauswahl kommt mit Login/Rollen in Phase 2.

## Getestet

Da kein Pi/ESP zur Verfügung stand, lief der komplette Test lokal gegen
`tools/fake_esp.py` (von Florians Branch, mittlerweile auf `main`):

- Gültiger Request → 200, `commands`/`config`/`demo` wie im Vertrag
- Falscher/fehlender API-Key, unbekanntes Gerät → 401
- Zu schnelle Requests (< 2 s Abstand) → 429
- `GET .../latest` liefert exakt das zurück, was zuletzt gesendet wurde
- Frontend + Backend zusammen über den Vite-Dev-Proxy durchgespielt

Dabei einen Bug gefunden und behoben: Beim allerersten Request eines neu
angelegten Geräts warf der Server einen 500er, weil die
SQLAlchemy-Objektdefaults für `pending_commands` erst beim DB-Flush
gesetzt werden, nicht beim Erzeugen des Python-Objekts. Fix: Defaults
jetzt explizit beim Anlegen gesetzt (`server/app/routers/readings.py`).

## Neu seit dem ESP-Firmware-Merge (PR #5, #3)

Mit der fertigen ESP32-Firmware und dem aktualisierten
`docs/hardware/verkabelung.md` kamen dazu:

- **Bodensensor gewechselt** auf kapazitiv (HW-390) – ändert nur die
  Hardware, nicht den API-Vertrag (`soil_moisture_pct`/`soil_moisture_raw`
  bleiben gleich). Keine Backend-/Frontend-Änderung nötig.
- **LED-Bar-Richtung bestätigt**: Rot = trocken, Grün = feucht – passt zu
  der Farbskala, die das Dashboard schon hatte.
- **Konkrete Fehlercodes aus der Firmware** (`smart_garden.ino`):
  `soil_out_of_range`, `light_read_failed`, `dht_read_failed`,
  `tank_empty`. Das Dashboard übersetzt diese jetzt in verständlichen
  Text (z. B. "Bodensensor außerhalb des Messbereichs" statt
  `soil_out_of_range`), mit dem Rohcode als Tooltip für unbekannte/neue
  Codes.
- `tools/fake_esp.py` ist jetzt auf `main` – kein manuelles Auschecken
  aus einem Feature-Branch mehr nötig zum Testen.

Der API-Vertrag selbst (`.claude/api-contract.md`) wurde **nicht**
geändert, daher war keine Vertragsanpassung nötig.

## Bekannte Lücken / Als Nächstes

- Hotspot-Skript auf echtem Pi verifizieren
- Phase 2 laut Plan: Recharts-Diagramme, Warnungen/Alerts, Login+Rollen
  (argon2 ist schon eingebunden), i18n (NL/DE/EN), Demo-Modus-Endpunkte +
  Dashboard-Bereich, KI-Stub
- API-Key + WLAN-Passwort müssen an Florian übergeben werden (siehe
  Blocker in `.claude/plan-esp.md`)
