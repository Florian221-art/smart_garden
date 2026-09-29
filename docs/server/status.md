# Backend, Dashboard and Deployment – Progress and Architecture

This document explains to the team (Nico, Florian) and to anyone reviewing the
project what has been built and tested so far for the backend, the dashboard
and the deployment. The short running status (done / next / blockers) is kept
in `.claude/plan-webserver.md`, as agreed; this file provides the more detailed
explanation behind it.

Last updated: 2026-09-28

## What is already working (Phase 1)

### 1. Wi-Fi hotspot (`deploy/`)

- `deploy/setup_hotspot.sh`: uses `nmcli` to create a NetworkManager hotspot
  named "SmartGarden" (WPA2) on `wlan0`. The Pi gets the IP `10.42.0.1`, the
  hotspot starts automatically at boot, and ufw rules are added for ports
  22/80/443/8000.
- `deploy/smart-garden-api.service`: systemd unit that runs the FastAPI server
  via uvicorn as the service `smart-garden-api` under the system user
  `smartgarden`.
- `deploy/README.md`: step-by-step guide for the first deployment.
- **Not yet tested on real Pi hardware** – no Pi was available in the
  development environment. This must be verified on the Pi once it is
  available.

### 2. Backend (`server/`)

Tech stack: FastAPI + Pydantic + SQLAlchemy (SQLite in WAL mode), argon2 for
hashing API keys. See also `server/README.md`.

- `POST /api/v1/readings` – implemented exactly according to
  `.claude/api-contract.md` v1.2:
  - Pydantic validates all body fields, including value ranges (e.g.
    `soil_moisture_pct` must be 0–100 or `null`).
  - The `X-API-Key` header is checked against the device's hashed key
    (argon2, which compares in constant time) → 401 if the key is wrong or
    missing, or the device is unknown.
  - Rate limit: at most 1 request per 2 s per device (in memory) → 429.
  - The response contains `commands`, `config` (default values from the
    contract) and `demo`. In Phase 1, `commands` was always empty/0/false and
    `demo` was always `null`; both are now filled by the demo mode and the
    device commands described further below.
  - `received_at` is set on the server (UTC), as required by the contract.
- `GET /api/v1/devices/{id}/latest` – the most recent reading of a device,
  used by the dashboard. Still **without authentication** (login/roles are
  planned for Phase 2) – intentionally, so that the dashboard already works.
- `GET /healthz` – simple liveness check.
- `python -m app.cli create-device <id> [--label ...]` – creates a device and
  prints its API key **once in plain text** (afterwards only the hash is
  stored in the database).
- Tables: `devices`, `readings` (all contract fields, including raw values,
  RSSI, uptime, errors, `demo_overrides`), `device_config` (configuration per
  device, currently only defaults) and `pending_commands` (one-time commands
  for the ESP32). A `demo_state` table was added later for the demo mode.

### 3. Dashboard (`web/`)

Tech stack: React 19 + Vite + TypeScript + Tailwind v4.

- The `Dashboard` component polls `GET /api/v1/devices/{id}/latest` every 5 s
  (Vite dev proxy `/api` → `localhost:8000`) and shows all current sensor
  data: soil moisture (including raw value), light (including raw value),
  temperature, air humidity, water tank, pump, device status
  (RSSI/uptime/firmware version) and errors.
- Error codes are translated into understandable German text (see the section
  "New since the ESP firmware merge" below) instead of only showing the raw
  code.
- The original red→green colour scale for soil moisture (matching the LED bar
  on the ESP32) has since been replaced by the monochrome design described
  below. The accessibility rule from the plan still applies: colour is
  **never the only carrier of information** – a number or text is always
  shown as well (WCAG requirement).
- The device ID is still hard-coded to `esp32-kuebel-01`; device selection
  will come with login/roles in Phase 2.

## Tested

Since no Pi or ESP32 was available, the complete test ran locally against
`tools/fake_esp.py` (originally from Florian's branch, now on `main`):

- Valid request → 200, `commands`/`config`/`demo` as specified in the contract
- Wrong/missing API key, unknown device → 401
- Requests sent too fast (< 2 s apart) → 429
- `GET .../latest` returns exactly what was last sent
- Frontend and backend exercised together through the Vite dev proxy

A bug was found and fixed during testing: on the very first request of a
newly created device, the server returned a 500 error because SQLAlchemy only
applies the column defaults for `pending_commands` when the session is
flushed to the database, not when the Python object is created. Fix: the
defaults are now set explicitly when the object is created
(`server/app/routers/readings.py`).

## New since the ESP firmware merge (PR #5, #3)

With the finished ESP32 firmware and the updated
`docs/hardware/verkabelung.md`, the following changes came in:

- **Soil sensor switched** to a capacitive sensor (HW-390) – this only changes
  the hardware, not the API contract (`soil_moisture_pct`/`soil_moisture_raw`
  stay the same). No backend or frontend change was needed.
- **LED bar direction confirmed**: red = dry, green = moist.
- **Concrete error codes from the firmware** (`smart_garden.ino`):
  `soil_out_of_range`, `light_read_failed`, `dht_read_failed`, `tank_empty`.
  The dashboard translates these into understandable German text (e.g.
  "Bodensensor außerhalb des Messbereichs" – "soil sensor out of measuring
  range" – instead of `soil_out_of_range`). Unknown or new codes are shown
  as the raw code.
- `tools/fake_esp.py` is now on `main` – it no longer has to be checked out
  manually from a feature branch for testing.

The API contract itself (`.claude/api-contract.md`) was **not** changed, so
no contract update was necessary.

## Charts in the dashboard (Phase 2, part 1)

Below the tiles there is now a **"Verlauf"** (history) section:

- Selectable time range: 1 hour, 24 hours, 7 days (refreshed every 30 s).
- Key figures for the selected range: water consumption (estimated from pump
  run time × flow rate), how often the device watered automatically, and the
  average soil moisture.
- **Soil moisture** chart with the watering threshold and the target value
  (dashed) and orange dots where the ESP32 watered on its own – the sawtooth
  pattern of drying out and watering is clearly visible.
- **Water tank** (estimate) with the warning threshold.
- **Water consumption** as bars per 5 minutes / hour / day.
- **Temperature, air humidity, light** (day/night cycle).
- Demo periods are shaded in all charts and labelled "DEMO". If a sensor
  fails, the line has a gap and the tooltip names the error.
- Every chart can be expanded into a table ("Als Tabelle anzeigen" – "show as
  table"); the colours have been checked for colour blindness and for light
  and dark mode.

Backend for this: `GET /api/v1/devices/{id}/readings?from=&to=&bucket=`
(averages per time bucket) and `GET /api/v1/devices/{id}/config`. To try it
out without hardware, `python -m app.dev_seed esp32-kuebel-01 --hours 48`
fills the **development** database with 48 hours of simulated history.

Fixed: timestamps were returned without a time zone, so the browser displayed
the time of the device's last report 2 hours off.

## Demo mode for the presentation (Phase 2, part 2)

The dashboard contains the **demo controls** (below the tiles, shown or hidden
via the "Demo" button at the top right). They override sensor values **on the
ESP32 itself** – so the real hardware (pump, LED bar, buzzer) reacts, as
specified in section 4 of the API contract. The pump's safety limits still
apply.

- **One-click scenarios** ("Situation vorspielen"): "Trockene Erde" (dry soil,
  12 % → the ESP32 waters), "Hitzewelle" (heat wave, 38 °C / 25 %), "Tank fast
  leer" (tank almost empty, 4 %), "Sensorausfall" (sensor failure, DHT11),
  "Nacht" (night, light 2 %).
- **Custom values** ("Eigene Werte einstellen"): sliders for soil moisture,
  light, temperature, air humidity and tank level, plus checkboxes for sensor
  failures.
- **Duration** ("Dauer") 1/2/5/10 min with a countdown; "Demo beenden" (end
  demo) switches back immediately. The ESP32 applies changes with its next
  report (every ~15 s) – the controls indicate as soon as the device reports
  the demo values.
- **Device** ("Gerät steuern"): "Gießen (5 s)" (water now for 5 s), "Tank
  aufgefüllt" (tank refilled), "Gerät finden" (find device – the LED
  flashes), "Piepen" (beep) – each delivered to the ESP32 exactly once.
- While a demo is running, **everyone** sees a banner at the top ("Demo
  läuft" – demo running); affected tiles are marked "DEMO", and the period is
  shaded in the history charts.

**Tip for the presentation:** after "Tank fast leer", the ESP32 keeps its tank
estimate of 4 % – press "Tank aufgefüllt" afterwards.

**Still open:** according to the plan, only admins should be allowed to do
this. There is no login/roles yet, so until then anyone on the hotspot Wi-Fi
can trigger the demo. Every change is logged in the server terminal.

Backend: `GET/PUT/DELETE /api/v1/devices/{id}/demo`,
`GET/POST /api/v1/devices/{id}/commands`.

## Dashboard on a phone

- **At home / during development:** run `npm run dev:mobil` instead of
  `npm run dev` (in the `web` folder). Vite then prints a line
  `Network: http://192.168.x.y:5173` – open this address on the phone (the
  phone must be on the same Wi-Fi as the Mac). The first time, macOS may ask
  whether "node" may accept incoming connections → allow it. The backend
  stays on the Mac (`127.0.0.1:8000`); Vite forwards `/api` to it.
- **On the Pi (hackathon):** `npm run build` creates `web/dist/`; FastAPI then
  serves it itself under `/`. Connect the phone to the "SmartGarden" Wi-Fi
  and open `http://10.42.0.1:8000`.
- **As an app:** on iPhone "Teilen → Zum Home-Bildschirm" (Share → Add to
  Home Screen), on Android "App installieren" (Install app) – with its own
  icon (a seedling with a water drop), it starts without the browser bar.
- Phone layout: tiles in two columns, touch targets ≥ 44 px, spacing around
  the notch and home indicator, demo controls in two columns, chart tooltips
  on tap. Checked as iPhone 13 (light/dark): no horizontal scrolling, no
  console errors.

**Caution:** with `dev:mobil`, **everyone on the same Wi-Fi** can see the
dashboard – including the demo controls, as long as there is no login. That
is fine at home; on other networks (school, hackathon) use the normal
`npm run dev` instead.

## Deployment to the Raspberry Pi

One command from the laptop: `./deploy/deploy_to_pi.sh <benutzer>@<pi-adresse>`
(i.e. `<user>@<pi-address>`; details in `deploy/README.md`). The script builds
the dashboard, copies everything to the Pi and sets up the server there
(systemd service on port 8000 that serves both the API and the dashboard),
the device and its API key, and the "SmartGarden" Wi-Fi hotspot.

- The hotspot is tuned for the ESP32: 2.4 GHz only (channel 6), pure WPA2,
  PMF disabled – the ESP32 often fails to connect to WPA3/5 GHz networks.
- Credentials (Wi-Fi password, API key) are **generated on the Pi** and stored
  only there (`/opt/smart-garden/zugangsdaten.txt`, readable by root only). The
  script shows them at the end and places a ready-to-use
  `firmware/smart_garden/secrets.h` on the laptop (ignored by Git). This way
  none of it ends up in the public repository.
- Repeatable: the database, API key and password are preserved.
- Tested in a Linux environment with Python 3.11 (as on Raspberry Pi OS
  Bookworm); Wi-Fi and systemd were only simulated there – the real hotspot
  still has to be verified on the Pi.

## New design (monochrome, light/dark)

The dashboard is meant to be understood by everyone, not just the team:

- **A status card at the top written in full sentences**: "Deiner Pflanze
  geht es gut" ("your plant is doing well") or a concrete action ("Wassertank:
  Fast leer – bald auffüllen" – "water tank: almost empty – refill soon"),
  plus online/offline and when the planter last reported.
- **Tiles with an interpretation** instead of bare numbers ("Gut feucht" –
  nicely moist, "Genug Wasser" – enough water, "Angenehm" – comfortable), and
  bars for soil moisture and tank level. Technical details (Wi-Fi signal,
  firmware, raw values) are listed further down under "Gerät" (device).
- **Monochrome**: one shade of green as the accent colour, otherwise grey
  tones. States are always shown as icon + text, never by colour alone.
- **Light / dark / follow the device** ("Hell" / "Dunkel" / "Wie das Gerät")
  via the switch at the top right; the browser remembers the choice.
- **Icons** from `lucide-react` (bundled with the build – no internet access
  needed).
- The **demo controls** only become visible after clicking "Demo".

## Known gaps / next steps

- Verify deployment and hotspot on a real Pi
- Rest of Phase 2: warnings/alerts, login + roles (argon2 is already
  included), i18n (NL/DE/EN), AI stub
- API key and Wi-Fi password are generated by `deploy_to_pi.sh` – Florian
  receives the `secrets.h` or the values directly (not via GitHub)
