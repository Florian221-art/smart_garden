# Deployment on the Raspberry Pi 5

This folder contains everything needed to run Smart Garden on a Raspberry Pi:
the FastAPI server (which also serves the dashboard), the device registration
with its API key, and a dedicated Wi-Fi hotspot that the ESP32 planter, phones
and laptops connect to.

## Quick start: one command from the laptop

Prerequisites:

- Raspberry Pi OS **Bookworm or newer** (64-bit) with SSH enabled
  (Raspberry Pi Imager → settings → "Enable SSH", or `sudo raspi-config`).
- The Pi is connected to the router via **Ethernet cable** (or to the laptop
  with internet sharing). `wlan0` is needed for the hotspot – once the hotspot
  is running, the Pi can no longer reach the internet via Wi-Fi.
- On the laptop: Node.js (to build the dashboard) and `ssh`.

In the repository folder on the laptop:

```
./deploy/deploy_to_pi.sh <benutzer>@<pi-adresse>
# z.B.
./deploy/deploy_to_pi.sh pi@raspberrypi.local
```

(`<benutzer>@<pi-adresse>` means `<user>@<pi-address>`.)

The script

1. builds the dashboard (`web/` → `web/dist/`),
2. copies `server/`, `web/dist/`, `deploy/` and `tools/` to the Pi via SSH,
3. runs `deploy/install_pi.sh` there with `sudo` (it may ask for the Pi
   user's password), which installs: system packages, the system user
   `smartgarden`, a virtual environment under `/opt/smart-garden/server/.venv`,
   the device `esp32-kuebel-01` with its API key, the systemd service
   `smart-garden-api` (port 8000, serves the API **and** the dashboard), and
   the "SmartGarden" Wi-Fi hotspot,
4. prints the **credentials** at the end (Wi-Fi password, API key, ready-made
   `CFG_*` lines for the ESP32),
5. and places a ready-to-use `firmware/smart_garden/secrets.h` on the laptop
   (ignored by Git – **do not commit it**, the repository is public).

The script can be run as often as needed (e.g. after code changes): the
database, API key and Wi-Fi password are preserved. To generate new ones:
`ssh <pi> 'sudo NEW_API_KEY=1 NEW_WIFI_PASSWORD=1 bash ~/smart-garden-upload/deploy/install_pi.sh'`
(then re-flash the ESP32).

## Afterwards

- **Dashboard:** connect the laptop/phone to the "SmartGarden" Wi-Fi → open
  `http://10.42.0.1:8000`. Alternatively, on the regular network via the Pi's
  LAN address: `http://<pi-ip>:8000` or `http://<hostname>.local:8000`.
- **ESP32:** `secrets.h` is already in the sketch folder → reopen the sketch
  in the Arduino IDE and upload it. The serial monitor shows
  "[CFG] Zugangsdaten aus secrets.h übernommen" (credentials loaded from
  secrets.h).
- **Show the credentials again** (on the Pi):
  `sudo cat /opt/smart-garden/zugangsdaten.txt`
- **Test without the ESP32:**
  `python tools/fake_esp.py --url http://10.42.0.1:8000 --key <api-key>`

## Useful commands on the Pi

```
sudo systemctl status smart-garden-api       # is the server running?
sudo journalctl -u smart-garden-api -f       # live log
sudo systemctl restart smart-garden-api      # restart
nmcli connection show --active               # is the hotspot active?
```

## Individual components (for doing things by hand)

- `deploy/setup_hotspot.sh <passwort>`: sets up only the hotspot (the
  argument is the Wi-Fi password; alternatively it is taken from the
  environment variable `SMART_GARDEN_WIFI_PASSWORD` or asked for
  interactively). 2.4 GHz (channel 6), pure WPA2 (RSN/CCMP), PMF disabled –
  so that the ESP32 connects reliably. Pi IP `10.42.0.1`.
- `deploy/smart-garden-api.service`: systemd unit (user `smartgarden`,
  port 8000).
- `python -m app.cli issue-key <id>`: creates the device or issues a new API
  key for it (existing readings are kept).
- Firewall: if `ufw` is installed, ports 22, 80, 443 and 8000 as well as
  DHCP/DNS on the hotspot interface are opened.
- `deploy/start-dashboard.command`: for development on a Mac, not for the Pi.
  Double-click it in Finder (or run it in a terminal) to start the backend
  and the Vite dev server together; an empty database is filled with 48 hours
  of test data. Stop with Ctrl + C.

## Notes / known limitations

- **No login yet**: anyone who has the Wi-Fi password can use the dashboard,
  including the demo controls. Only share the password with the team
  (login/roles will follow).
- If the Pi is connected to the network only via Wi-Fi, `install_pi.sh`
  aborts with an explanation. With `FORCE_WLAN=1 ./deploy/deploy_to_pi.sh …`
  it proceeds anyway – the credentials are then displayed before the hotspot
  cuts the SSH connection.
- Planned for Phase 3: Caddy (HTTPS via `tls internal`), Ollama, daily SQLite
  backup.
