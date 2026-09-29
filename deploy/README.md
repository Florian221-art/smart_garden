# Deployment auf dem Raspberry Pi 5

## Kurzfassung: ein Befehl vom Laptop aus

Voraussetzungen:

- Raspberry Pi OS **Bookworm oder neuer** (64-bit), SSH aktiviert
  (Raspberry Pi Imager → Einstellungen → „SSH aktivieren“, oder `sudo raspi-config`).
- Pi hängt per **LAN-Kabel** am Router (oder am Laptop mit Internetfreigabe).
  `wlan0` wird für den Hotspot gebraucht – über WLAN kann der Pi danach nicht mehr ins Internet.
- Auf dem Laptop: Node.js (zum Bauen des Dashboards) und `ssh`.

Im Repo-Ordner auf dem Laptop:

```
./deploy/deploy_to_pi.sh <benutzer>@<pi-adresse>
# z.B.
./deploy/deploy_to_pi.sh pi@raspberrypi.local
```

Das Skript

1. baut das Dashboard (`web/` → `web/dist/`),
2. kopiert `server/`, `web/dist/`, `deploy/`, `tools/` per SSH auf den Pi,
3. startet dort `deploy/install_pi.sh` mit `sudo` (fragt evtl. nach dem Pi-Passwort):
   Pakete, Systembenutzer `smartgarden`, venv unter `/opt/smart-garden/server/.venv`,
   Gerät `esp32-kuebel-01` + API-Key, systemd-Dienst `smart-garden-api` (Port 8000,
   liefert API **und** Dashboard), WLAN-Hotspot „SmartGarden“,
4. zeigt am Ende die **Zugangsdaten** (WLAN-Passwort, API-Key, fertige `CFG_*`-Zeilen für den ESP32)
5. und legt eine fertige `firmware/smart_garden/secrets.h` auf dem Laptop ab
   (von Git ignoriert – **nicht committen**, das Repo ist öffentlich).

Beliebig oft wiederholbar (z. B. nach Code-Änderungen): Datenbank, API-Key und
WLAN-Passwort bleiben erhalten. Neu erzeugen:
`ssh <pi> 'sudo NEW_API_KEY=1 NEW_WIFI_PASSWORD=1 bash ~/smart-garden-upload/deploy/install_pi.sh'`
(danach ESP neu flashen).

## Danach

- **Dashboard:** Laptop/Handy ins WLAN „SmartGarden“ → `http://10.42.0.1:8000`.
  Alternativ im normalen Netz über die LAN-Adresse des Pi: `http://<pi-ip>:8000`
  bzw. `http://<hostname>.local:8000`.
- **ESP32:** `secrets.h` liegt schon im Sketch-Ordner → Arduino IDE neu öffnen, hochladen.
  Im seriellen Monitor erscheint „[CFG] Zugangsdaten aus secrets.h übernommen“.
- **Zugangsdaten erneut anzeigen** (auf dem Pi): `sudo cat /opt/smart-garden/zugangsdaten.txt`
- **Test ohne ESP:** `python tools/fake_esp.py --url http://10.42.0.1:8000 --key <api-key>`

## Nützliche Befehle auf dem Pi

```
sudo systemctl status smart-garden-api       # läuft der Server?
sudo journalctl -u smart-garden-api -f       # Live-Log
sudo systemctl restart smart-garden-api      # neu starten
nmcli connection show --active               # ist der Hotspot aktiv?
```

## Einzelteile (falls man etwas von Hand machen will)

- `deploy/setup_hotspot.sh <passwort>`: nur den Hotspot einrichten. 2,4 GHz (Kanal 6),
  reines WPA2 (RSN/CCMP), PMF aus – damit der ESP32 sich sicher verbindet. Pi-IP `10.42.0.1`.
- `deploy/smart-garden-api.service`: systemd-Unit (Benutzer `smartgarden`, Port 8000).
- `python -m app.cli issue-key <id>`: Gerät anlegen bzw. neuen API-Key ausgeben (Messwerte bleiben).
- Firewall: Falls `ufw` installiert ist, werden 22, 80, 443, 8000 sowie DHCP/DNS im Hotspot freigegeben.

## Hinweise / bekannte Grenzen

- Noch **kein Login**: Wer das WLAN-Passwort hat, kann das Dashboard inkl. Demo-Steuerung
  benutzen. Passwort nur ans Team geben (Login/Rollen folgen).
- Hängt der Pi nur per WLAN am Netz, bricht `install_pi.sh` mit einem Hinweis ab.
  Mit `FORCE_WLAN=1 ./deploy/deploy_to_pi.sh …` geht es trotzdem – die Zugangsdaten
  werden dann angezeigt, bevor der Hotspot die SSH-Verbindung kappt.
- Ab Phase 3 kommen dazu: Caddy (HTTPS via `tls internal`), Ollama, tägliches SQLite-Backup.
