# Smart Garden – Web Dashboard

React dashboard for the Smart Garden planter. It shows the current sensor
readings of the ESP32 planter, the history as charts, and provides the demo
controls used during the presentation. The dashboard talks only to the
FastAPI server in `server/` (all requests go to `/api/...`); it never talks to
the ESP32 directly.

The user interface is in German. Labels quoted below are given verbatim, with
their English meaning.

## Tech stack

- React 19, TypeScript, Vite
- Tailwind CSS v4 (via `@tailwindcss/vite`)
- Recharts for the charts
- `lucide-react` for icons (bundled into the build, so no internet access is
  needed at runtime)
- Oxlint for linting

## Features

- **Status card** at the top, written in full sentences, e.g. "Deiner Pflanze
  geht es gut" (your plant is doing well), "Bitte kurz nachsehen" (please take
  a quick look) or "Deine Pflanze braucht Hilfe" (your plant needs help),
  together with online/offline state and the time of the last report.
- **Tiles** for "Bodenfeuchte" (soil moisture), "Wassertank" (water tank),
  "Temperatur" (temperature), "Luftfeuchte" (air humidity), "Licht" (light)
  and "Bewässerung" (watering), each with a plain-language interpretation such
  as "Gut feucht" (nicely moist) or "Fast leer – bald auffüllen" (almost
  empty – refill soon). The latest reading is polled every 5 s from
  `GET /api/v1/devices/{id}/latest`.
- **Device card** ("Gerät"): last report ("Letzte Meldung"), Wi-Fi signal
  ("WLAN-Signal"), uptime, firmware version and raw sensor values.
- **Error codes** from the firmware (`soil_out_of_range`, `light_read_failed`,
  `dht_read_failed`, `tank_empty`) are translated into readable German text
  (`src/errorMessages.ts`); unknown codes are shown as-is.
- **History** ("Verlauf"): time ranges of 1 hour, 24 hours and 7 days,
  refreshed every 30 s from `GET /api/v1/devices/{id}/readings`. Charts for
  soil moisture (with watering threshold, target value and automatic watering
  events), water tank (estimate), water consumption, temperature, air
  humidity and light. Demo periods are shaded and labelled "DEMO". Every
  chart can also be shown as a table ("Als Tabelle anzeigen").
- **Demo controls** (visible after clicking "Demo"): one-click scenarios
  ("Trockene Erde", "Hitzewelle", "Tank fast leer", "Sensorausfall",
  "Nacht" – dry soil, heat wave, tank almost empty, sensor failure, night),
  custom values ("Eigene Werte einstellen"), a duration of 1/2/5/10 min, and
  one-time device commands ("Gießen (5 s)", "Tank aufgefüllt", "Gerät
  finden", "Piepen" – water for 5 s, tank refilled, find device, beep). These
  use `/api/v1/devices/{id}/demo` and `/api/v1/devices/{id}/commands`.
- **Accessibility and theming:** states are always shown as icon + text,
  never by colour alone; light/dark/system theme ("Hell" / "Dunkel" / "Wie das
  Gerät") is remembered in the browser; the layout is optimised for phones.
- **Installable as a web app** (web app manifest and icons in `public/`).

The device ID is currently fixed to `esp32-kuebel-01` (`src/Dashboard.tsx`).
There is no login yet, so anyone who can reach the dashboard can also use the
demo controls.

## Development

Requires Node.js and the backend running on `127.0.0.1:8000` (see
`server/README.md`). The Vite dev server proxies `/api` to it
(`vite.config.ts`).

```
cd web
npm install
npm run dev          # http://localhost:5173, only reachable from this computer
npm run dev:mobil    # same, but reachable from other devices on the Wi-Fi (e.g. a phone)
```

With `npm run dev:mobil`, everyone on the same network can open the dashboard,
including the demo controls – only use it on trusted networks.

On a Mac, `deploy/start-dashboard.command` starts backend and frontend
together and fills an empty database with test data.

## Scripts

| Command | Purpose |
| --- | --- |
| `npm run dev` | Vite dev server on port 5173 (local only) |
| `npm run dev:mobil` | Vite dev server reachable on the local network (`vite --host`) |
| `npm run build` | Type-check (`tsc -b`) and build into `web/dist/` |
| `npm run lint` | Run Oxlint |
| `npm run preview` | Preview the production build |

## Production

`npm run build` creates `web/dist/`. The FastAPI server serves this folder
under `/` automatically if it exists, so no separate web server is needed on
the Raspberry Pi. `deploy/deploy_to_pi.sh` builds and deploys it (see
`deploy/README.md`).
