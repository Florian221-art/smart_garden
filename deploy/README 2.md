# Deployment auf dem Raspberry Pi 5

Reihenfolge (Phase 1):

1. **Hotspot einrichten** (einmalig, auf dem Pi):
   ```
   sudo ./deploy/setup_hotspot.sh
   ```
   Erzeugt WLAN "SmartGarden" (WPA2) auf `wlan0`, Pi-IP `10.42.0.1`.
   Internet fuer den Pi laeuft ueber Ethernet (Updates, spaeter Ollama-Modell).
   Passwort **nicht** committen - Florian bekommt es fuer `secrets.h` auf dem ESP.

2. **Server-Code auf den Pi bringen** (Phase 1: manuell/`git clone`, spaeter ggf. Deploy-Skript):
   ```
   sudo mkdir -p /opt/smart-garden
   sudo chown $USER /opt/smart-garden
   git clone <repo-url> /opt/smart-garden
   cd /opt/smart-garden/server
   python3 -m venv .venv
   ./.venv/bin/pip install -r requirements.txt
   ./.venv/bin/python -m app.cli create-device esp32-kuebel-01
   ```

3. **systemd-Service installieren:**
   ```
   sudo useradd --system --no-create-home smartgarden || true
   sudo chown -R smartgarden /opt/smart-garden
   sudo cp deploy/smart-garden-api.service /etc/systemd/system/
   sudo systemctl daemon-reload
   sudo systemctl enable --now smart-garden-api
   sudo systemctl status smart-garden-api
   ```

4. **Firewall (ufw):** 22 (SSH), 80/443 (ab Phase 3, Caddy), 8000 (Phase 1, direkter FastAPI-Zugriff).
   Wird von `setup_hotspot.sh` mit gesetzt, falls ufw installiert ist.

5. **Test ohne Hardware:** `python tools/fake_esp.py --url http://10.42.0.1:8000 --key <api-key>`
   (vom Laptop im Hotspot-WLAN aus, oder lokal gegen `localhost:8000` waehrend der Entwicklung).

Ab Phase 3 kommen dazu: Caddy (HTTPS via `tls internal`), Ollama-Installation,
taegliches SQLite-Backup. Diese Datei wird dann erweitert.
