# Smart Garden – an intelligent planter

Hackathon project (**Energy & Mobility EuRegio Hackathon 2026**, 48 hours, September 2026) by **Florian Schoenen** and **Nico Steins**.
The original challenge description is in [`docs/Intelligenter Gemüsegarten Pflanzkübel DE.pdf`](docs/Intelligenter%20Gemüsegarten%20Pflanzkübel%20DE.pdf) (German).

**Want to see it running? No hardware needed:** clone the repo and run `bash deploy/demo.sh` – see [Try it in two minutes](#try-it-in-two-minutes-no-hardware-needed).

## Overview

### The problem

A vegetable planter on a school campus needs regular, measured watering. Too little water and the plants dry out; too much wastes water. The people looking after it (students, teachers) need to see at a glance whether the plant is fine or whether something needs to be done – without any technical knowledge.

### Our solution

An ESP32 microcontroller in the planter measures soil moisture, light, air temperature and humidity. It waters the plant on its own, in small bursts and only as much as needed, shows the soil condition directly on the planter with an LED bar, and sounds a buzzer when there is a problem. All readings are sent over Wi-Fi to a Raspberry Pi, which stores them and serves a web dashboard that explains the plant's state in plain sentences. An AI module for anomaly detection and water-demand forecasting is designed but not yet implemented (see [Status and known limitations](#status-and-known-limitations)).

The planter keeps working without the Pi: measuring, the LED bar, the buzzer and automatic watering all run locally on the ESP32; only sending data stops.

### Features

- **Sensing:** capacitive soil moisture, light (LDR), temperature and humidity (DHT11). Each analog value is the median of 5 samples; implausible values are reported as sensor errors.
- **Water-saving automatic watering:** when soil moisture falls below a threshold (default 30 %), the pump runs in short bursts (default 0.5 s) with a pause in between (default 300 s) so the water can soak in, until a target moisture (default 55 %) is reached. A daily pump limit applies.
- **Water tank estimate without a level sensor:** remaining water = capacity − pump run time × measured flow rate. The estimate is stored in flash and survives restarts. At a tank level of 5 % or less the pump does not run (dry-run protection).
- **Plausibility check:** if watering has no measurable effect (sensor not in the soil, hose misplaced, pump sucking air), automatic watering is locked and an alarm is raised.
- **On-site feedback:** a 10-segment LED bar (the drier the soil, the more LEDs light up – understandable without reading), a buzzer for new problems, and an onboard status LED for Wi-Fi.
- **Dashboard (React):** a status card in full sentences, tiles with a plain-language rating instead of bare numbers, history charts (1 h / 24 h / 7 days) for moisture, tank, water use, temperature, humidity and light, light/dark theme, a mobile layout, and it can be installed as a home-screen app.
- **Demo mode for presentations:** sensor values can be overridden and sensor failures forced, either from the dashboard or from the ESP32's serial console. The override happens on the ESP32, so the real hardware reacts (pump, LED bar, buzzer). Demo data is flagged and is not used for AI training.
- **Security by design:** per-device API keys (stored as argon2 hashes), a dedicated WPA2 hotspot, hard pump limits compiled into the firmware, a watchdog, and no secrets in the repository (see [Security highlights](#security-highlights)).

## Try it in two minutes (no hardware needed)

The demo runs the complete software stack on your own computer. A simulated planter (`tools/fake_esp.py`) behaves like the real ESP32 firmware: soil dries out over a simulated day/night cycle, the pump waters it in short bursts, and the water tank empties. The real server and the real dashboard show it live.

**Requirements:** Python 3.10+, Node.js 20+ with npm, macOS or Linux (on Windows use WSL). The first run downloads dependencies and takes about a minute.

```bash
git clone https://github.com/Florian221-art/smart_garden.git
cd smart_garden
bash deploy/demo.sh
```

The script builds the dashboard, creates a throw-away demo database with 48 hours of simulated history, starts the server and the simulated planter, and opens **http://localhost:8000** in your browser. Stop it with `Ctrl+C`. Nothing is installed outside the repository folder and no real database is touched (demo data lives in the git-ignored `.demo/` folder).

**What to try in the dashboard**

1. Watch the status card and the tiles update – the simulated planter reports every 2 s in time-lapse (60 simulated seconds per real second).
2. Open the history charts (1 h / 24 h / 7 days) to see the 48 hours of pre-generated data: day/night curves, drying soil and automatic watering events.
3. Click **Demo** and trigger a scenario such as dry soil, heat wave, almost empty tank or sensor failure. The simulated device reacts exactly like the real hardware would: it starts the pump, and the dashboard explains the problem in plain sentences. The dashboard UI itself is German.
4. Browse the interactive API documentation at http://localhost:8000/docs.

**Options** (environment variables): `PORT=8010` to use another port, `DEMO_SPEED=120` for a faster time-lapse, `NO_OPEN=1` to not open the browser.

<details>
<summary>Manual steps instead of the script</summary>

```bash
# Terminal 1 – backend (from the repository root)
cd server
python3 -m venv .venv && ./.venv/bin/pip install -r requirements.txt
./.venv/bin/python -m app.dev_seed esp32-kuebel-01 --hours 48   # test data, prints the API key once
./.venv/bin/uvicorn app.main:app --reload                        # http://127.0.0.1:8000

# Terminal 2 – dashboard with hot reload
cd web && npm install && npm run dev                             # http://localhost:5173

# Terminal 3 – simulated planter
python3 tools/fake_esp.py --url http://127.0.0.1:8000 --key <api-key> --interval 2 --speed 60
```

</details>

## Architecture

```
 Planter                                 Raspberry Pi 5 (own Wi-Fi "SmartGarden", 10.42.0.1)
 ┌─────────────────────────────┐         ┌───────────────────────────────────────────────────┐
 │ ESP32                       │  Wi-Fi  │ FastAPI server ── SQLite                        │
 │  ├ Soil moisture (capacit.) │ ──────► │   POST /api/v1/readings  (X-API-Key)            │
 │  ├ Light (LDR)              │ ◄────── │   Response: config · commands · demo            │
 │  ├ Temp./humidity (DHT11)   │         │ AI module (Python) – planned                    │
 │  ├ LED bar (moisture)       │         │ Dashboard (React + Tailwind) ◄── phone/laptop   │
 │  ├ Buzzer (alarm)           │         └───────────────────────────────────────────────────┘
 │  └ Relay → 12 V pump        │
 └─────────────────────────────┘
```

1. **ESP32 (planter):** measures every 2 s, controls the pump locally and sends a reading every `interval_s` seconds via `POST /api/v1/readings` (firmware default 10 s; the value from the server's `config` takes precedence once connected). It only makes outgoing connections; there is no web server on the ESP32.
2. **Wi-Fi hotspot:** the Raspberry Pi 5 runs its own NetworkManager hotspot "SmartGarden" (2.4 GHz, WPA2, Pi address `10.42.0.1`), independent of the venue Wi-Fi. The ESP32 and the viewers' phones and laptops connect to it.
3. **FastAPI server + SQLite:** checks the device's API key, validates every field, stores the reading and answers with `config` (thresholds, pump and tank settings), one-time `commands` (e.g. water now, tank refilled, identify, beep) and optional `demo` overrides. The ESP32 applies these from the response.
4. **Dashboard:** a React/Vite/Tailwind single-page app, built to static files and served by the same FastAPI service on port 8000. It reads the latest values and the history through the server's REST API.
5. **AI module (planned):** Python (scikit-learn) for water-demand forecasting and anomaly detection, optionally a local chatbot via Ollama. The design is in [`.claude/plan-webserver.md`](.claude/plan-webserver.md); there is no `ai/` code in the repository yet.

The interface between the ESP32 and the server (JSON fields, commands, demo mode, error codes) is defined in [`.claude/api-contract.md`](.claude/api-contract.md) (version 1.2).

## Hardware

| Component | Purpose | ESP32 pin |
|---|---|---|
| ESP32 DevKit V1 (ESP-WROOM-32, 30 pins, CP2102 USB chip) | Controller | – |
| Capacitive Soil Moisture Sensor v2.0 (HW-390) | Soil moisture | GPIO34 |
| LDR light sensor module | Light | GPIO35 |
| Grove Temperature & Humidity Sensor v1.2 (DHT11) | Air temperature and humidity | GPIO4 |
| Grove LED Bar v2.0 | Moisture display | GPIO18 (DI, data), GPIO19 (DCKI, clock) |
| Buzzer LF-PB30W35B | Alarm | GPIO26 |
| Grove Relay + 12 V pump | Watering | GPIO27 |
| Onboard BOOT button | Hold 3 s = tank refilled | GPIO0 |
| Onboard LED | Wi-Fi status | GPIO2 |
| Raspberry Pi 5 | Hotspot, server, database, dashboard | – |

All modules run on 3.3 V; the 12 V pump circuit is switched only through the relay's screw terminal and never touches an ESP32 pin. Full wiring plan and commissioning order: [`docs/hardware/verkabelung.md`](docs/hardware/verkabelung.md).

## Repository structure

| Folder | Contents | Owner | Docs |
|---|---|---|---|
| `firmware/smart_garden/` | ESP32 firmware (C++, Arduino IDE) | Florian | [README](firmware/smart_garden/README.md) |
| `tools/` | `fake_esp.py`: ESP32 simulator for testing without hardware | Florian | [README](tools/README.md) |
| `docs/hardware/` | Pinout, wiring, ESP32 security analysis | Florian | [Wiring](docs/hardware/verkabelung.md) · [Security](docs/hardware/sicherheit-esp.md) |
| `server/` | FastAPI backend, SQLite database | Nico | [`docs/server/`](docs/server/status.md) |
| `web/` | Dashboard (React, Vite, Tailwind) | Nico | [`docs/server/`](docs/server/status.md) |
| `ai/` | AI module (Python) – planned, not yet in the repository | Nico | [`.claude/plan-webserver.md`](.claude/plan-webserver.md) |
| `deploy/` | `demo.sh` (one-command demo on your computer), Raspberry Pi setup: hotspot, systemd service, deploy script | Nico | [README](deploy/README.md) |
| `.claude/` | Plans, rules and the API contract for the two Claude Code instances used during development | both | [Rules](.claude/CLAUDE.md) |

## Quick start

1. **Set up the Pi:** from a laptop, run `./deploy/deploy_to_pi.sh <user>@<pi-address>` as described in [`deploy/README.md`](deploy/README.md). The script builds the dashboard, installs the server as a systemd service, creates the hotspot and a device with an API key, and writes a ready-made `firmware/smart_garden/secrets.h` on the laptop.
2. **Wire the planter** according to [`docs/hardware/verkabelung.md`](docs/hardware/verkabelung.md).
3. **Upload the firmware** following [`firmware/smart_garden/README.md`](firmware/smart_garden/README.md). The Wi-Fi password and API key belong in `secrets.h`, never in the repository.
4. **Calibrate:** enter `cal soil dry` and `cal soil wet` in the serial monitor, then measure the pump flow rate.
5. **Open the dashboard:** join the "SmartGarden" Wi-Fi and open `http://10.42.0.1:8000`.
6. **Test without hardware:** run `bash deploy/demo.sh` on your computer (see [Try it in two minutes](#try-it-in-two-minutes-no-hardware-needed)), or point the simulator at the Pi: `python tools/fake_esp.py --url http://10.42.0.1:8000 --key <api-key>`

## How the solution covers the challenge

| Requirement | Implementation |
|---|---|
| Sensor data (moisture, light, temperature, water level) | 4 sensors on the ESP32. There is no level sensor; the firmware estimates the water level from the pump run time. |
| Avoid wasting water | Watering in short bursts with pauses up to a target moisture, a daily limit, and a plausibility check that stops watering when it has no effect. |
| Warnings | Buzzer and LED bar on the planter, status messages on the dashboard. |
| Accessibility | The LED bar is understandable without reading (more LEDs = drier). The dashboard explains the state in full sentences, never relies on colour alone (always symbol + text) and uses touch targets of at least 44 px. The dashboard UI is currently in German; NL/DE/EN translations are planned. |
| AI | Planned: anomaly detection and water-demand forecasting (`ai/`, see `.claude/plan-webserver.md`). |
| Security | API key per device, WPA2 hotspot, hard pump limits in the ESP32, watchdog, no secrets in the repository, threat model ([ESP32 part](docs/hardware/sicherheit-esp.md)). |
| Privacy | No camera, no microphone, no personal data at the planter. |
| Demo | Demo mode: override sensor values and trigger faults, from the dashboard or the serial console. |

## Security highlights

- **Device authentication:** every reading carries an `X-API-Key` header. The server stores only an argon2 hash of each key and rejects wrong or missing keys and unknown devices with HTTP 401. A per-device rate limit (at most 1 request per 2 s) answers with HTTP 429.
- **Input validation on both sides:** the server validates all fields and value ranges (Pydantic); the firmware clamps every setting it receives from the server to safe ranges.
- **Hard limits in the firmware** that the server cannot change: at most 15 s per pump run, a daily limit of at most 300 s, at least 10 s pause before every start (including dashboard and demo commands), no pumping at a tank level of 5 % or less, and demo overrides expire after at most 600 s.
- **Fail-safe behaviour:** a 20 s watchdog restarts the ESP32 if the main loop hangs, switching the relay off first. The relay pin is LOW at boot, so the pump cannot start on power-up. No data is sent while the pump runs, so a slow server cannot delay switching it off.
- **Secrets stay out of the repository:** the Wi-Fi password and API keys live in `secrets.h` (firmware) or `.env`, both listed in `.gitignore`. The deploy script generates the credentials on the Pi. The firmware never prints the password or key.
- **Isolated network:** the Pi's own WPA2 hotspot; if `ufw` is installed, only the required ports are opened.

The full threat model of the ESP32 part (STRIDE, OWASP IoT Top 10) is in [`docs/hardware/sicherheit-esp.md`](docs/hardware/sicherheit-esp.md).

## Status and known limitations

- **No login yet:** anyone connected to the hotspot can use the dashboard, including the demo controls and device commands. Login with admin/reader roles is planned.
- **HTTP for now:** over HTTP the API key can be read by others on the Wi-Fi. HTTPS via Caddy (`tls internal`) is prepared in the firmware (`SERVER_CA_CERT`) but not yet deployed.
- **AI module:** designed, not yet implemented.
- **Dashboard language:** German only so far; NL/DE/EN via translation keys is planned.
- **No level sensor and no clock:** the tank estimate is only correct if the flow rate was measured and `refill` is triggered after every refill. The daily pump limit applies per 24 h of uptime and restarts after a reboot.

## Team

| Name | Responsibility |
|---|---|
| **Florian Schoenen** | ESP32 firmware, hardware and wiring, test tools, ESP32 security analysis |
| **Nico Steins** | Server/API, database, dashboard, AI module, Raspberry Pi deployment including the Wi-Fi hotspot |

During development each of us worked with a separate Claude Code instance; their shared rules, plans and the API contract are in `.claude/`.

## Contribution rules

- **Never push directly to `main`.** Always use a branch and a pull request.
- **Never commit secrets.** The Wi-Fi password and API keys belong in `secrets.h` or `.env`; both are in `.gitignore`.
- Change the API only through a `[CONTRACT]` pull request; both team members must approve.
- More rules: [`.claude/CLAUDE.md`](.claude/CLAUDE.md)

License: GNU GPL v3, see [LICENSE](LICENSE).
