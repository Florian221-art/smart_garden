#!/usr/bin/env bash
# Komplett-Installation von Smart Garden auf dem Raspberry Pi (idempotent).
#
# Normalerweise NICHT direkt aufrufen, sondern vom Laptop aus:
#   ./deploy/deploy_to_pi.sh <benutzer>@<pi-adresse>
# Das baut das Dashboard, kopiert alles auf den Pi und startet dann dieses Skript.
#
# Direkt auf dem Pi (Repo-Kopie mit fertigem web/dist/ muss vorhanden sein):
#   sudo bash deploy/install_pi.sh
#
# Was passiert:
#   1. Pakete (python3-venv ...) installieren, Systembenutzer "smartgarden" anlegen
#   2. server/ + web/dist/ nach /opt/smart-garden kopieren, venv + Abhaengigkeiten
#   3. Geraet esp32-kuebel-01 + API-Key anlegen (bzw. vorhandenen Key behalten)
#   4. systemd-Dienst smart-garden-api starten (Port 8000, liefert auch das Dashboard)
#   5. WLAN-Hotspot "SmartGarden" fuer den ESP32 einrichten (deploy/setup_hotspot.sh)
#   6. Zugangsdaten ausgeben
#
# Zugangsdaten landen NUR auf dem Pi:
#   /opt/smart-garden/zugangsdaten.txt          (nur root lesbar)
#   ~<sudo-benutzer>/smart-garden-secrets.h     (nur dieser Benutzer lesbar, fuer den ESP)
# Niemals ins Repo committen (das Repo ist oeffentlich).
#
# Neuen API-Key / neues WLAN-Passwort erzwingen:
#   sudo NEW_API_KEY=1 NEW_WIFI_PASSWORD=1 bash deploy/install_pi.sh

set -euo pipefail

SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST="/opt/smart-garden"
SVC_USER="smartgarden"
SVC_NAME="smart-garden-api"
DEVICE_ID="${DEVICE_ID:-esp32-kuebel-01}"
SSID="SmartGarden"
PI_IP="10.42.0.1"
PORT="8000"
CRED="${DEST}/zugangsdaten.txt"
DB_URL="sqlite:///${DEST}/server/smart_garden.db"
VENV="${DEST}/server/.venv"

step() { echo; echo "==> $*"; }
die()  { echo "FEHLER: $*" >&2; exit 1; }

[[ "${EUID}" -eq 0 ]] || die "Bitte mit sudo ausfuehren: sudo bash $0"
[[ -f "${SRC}/server/requirements.txt" ]] || die "server/ nicht gefunden unter ${SRC}"
[[ -f "${SRC}/web/dist/index.html" ]] || die "web/dist/ fehlt - bitte ueber deploy_to_pi.sh (baut das Dashboard) starten oder vorher 'npm run build' in web/ ausfuehren."
command -v nmcli >/dev/null 2>&1 || die "NetworkManager (nmcli) fehlt. Raspberry Pi OS Bookworm oder neuer verwenden."

# --- Laeuft die Internet-/SSH-Verbindung ueber wlan0? Dann wuerde der Hotspot sie kappen.
WLAN_UPLINK=0
if ip route get 1.1.1.1 2>/dev/null | grep -q " dev wlan0 "; then
  WLAN_UPLINK=1
fi
if [[ "${WLAN_UPLINK}" -eq 1 && "${FORCE_WLAN:-0}" != "1" ]]; then
  cat >&2 <<MSG
FEHLER: Der Pi haengt gerade ueber WLAN (wlan0) am Netz.
Der Hotspot braucht wlan0 fuer sich - danach haette der Pi kein Internet mehr
und deine SSH-Verbindung wuerde abbrechen.

Loesung: Pi per LAN-Kabel (Ethernet) an Router/Laptop anschliessen und neu starten.

Wenn du es trotzdem willst (Pi danach ohne Internet, SSH bricht am Ende ab,
Zugangsdaten werden VORHER angezeigt):
  sudo FORCE_WLAN=1 bash $0
MSG
  exit 2
fi

step "1/6 Pakete installieren"
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq python3-venv python3-pip curl sqlite3 >/dev/null
PY_OK=$(python3 -c 'import sys; print(int(sys.version_info >= (3, 10)))')
[[ "${PY_OK}" == "1" ]] || die "Python >= 3.10 noetig, gefunden: $(python3 --version)"

if ! id -u "${SVC_USER}" >/dev/null 2>&1; then
  useradd --system --no-create-home --shell /usr/sbin/nologin "${SVC_USER}"
fi

step "2/6 Code nach ${DEST} kopieren"
mkdir -p "${DEST}/server" "${DEST}/web"
rm -rf "${DEST}/server/app" "${DEST}/web/dist" "${DEST}/deploy" "${DEST}/tools"
tar -C "${SRC}" \
  --exclude='.venv' --exclude='*.db' --exclude='*.db-*' --exclude='__pycache__' \
  --exclude='* 2' --exclude='* 2.*' --exclude='._*' --exclude='.DS_Store' \
  -cf - server web/dist deploy $( [[ -d "${SRC}/tools" ]] && echo tools ) \
  | tar -C "${DEST}" -xf -

step "3/6 Python-Umgebung einrichten (dauert beim ersten Mal ein paar Minuten)"
if [[ ! -x "${VENV}/bin/python" ]]; then
  python3 -m venv "${VENV}"
fi
"${VENV}/bin/pip" install --quiet --upgrade pip
"${VENV}/bin/pip" install --quiet -r "${DEST}/server/requirements.txt"
chown -R "${SVC_USER}:${SVC_USER}" "${DEST}/server" "${DEST}/web"

run_cli() {
  (cd "${DEST}/server" && sudo -u "${SVC_USER}" env SMART_GARDEN_DB_URL="${DB_URL}" \
     "${VENV}/bin/python" -m app.cli "$@")
}

step "4/6 Geraet ${DEVICE_ID} + Zugangsdaten"
WIFI_PASSWORD=""
API_KEY=""
if [[ -f "${CRED}" ]]; then
  WIFI_PASSWORD=$(sed -n 's/^WLAN_PASSWORT=//p' "${CRED}" | head -n1)
  API_KEY=$(sed -n 's/^API_KEY=//p' "${CRED}" | head -n1)
fi
if [[ -z "${WIFI_PASSWORD}" || "${NEW_WIFI_PASSWORD:-0}" == "1" ]]; then
  # 16 Zeichen, ohne leicht verwechselbare Zeichen (0/O, 1/l/I)
  WIFI_PASSWORD=$(python3 -c 'import secrets; a="ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnpqrstuvwxyz23456789"; print("".join(secrets.choice(a) for _ in range(16)))')
  echo "   Neues WLAN-Passwort erzeugt."
else
  echo "   Vorhandenes WLAN-Passwort wird weiterverwendet."
fi
if [[ -n "${API_KEY}" && "${NEW_API_KEY:-0}" != "1" ]] && run_cli has-device "${DEVICE_ID}"; then
  echo "   Vorhandener API-Key wird weiterverwendet."
else
  API_KEY=$(run_cli issue-key "${DEVICE_ID}" --label "Kuebel 1" | tail -n1)
  [[ -n "${API_KEY}" ]] || die "API-Key konnte nicht erzeugt werden."
  echo "   Neuer API-Key erzeugt."
fi

umask 077
cat > "${CRED}" <<CREDS
# Smart Garden - Zugangsdaten (NICHT committen, NICHT weitergeben ausser ans Team)
# Erzeugt von deploy/install_pi.sh am $(date '+%Y-%m-%d %H:%M')
WLAN_SSID=${SSID}
WLAN_PASSWORT=${WIFI_PASSWORD}
SERVER_URL=http://${PI_IP}:${PORT}
DEVICE_ID=${DEVICE_ID}
API_KEY=${API_KEY}
CREDS
chmod 600 "${CRED}"; chown root:root "${CRED}"

SECRETS_H_CONTENT=$(cat <<SH
// secrets.h - erzeugt von deploy/install_pi.sh auf dem Pi ($(date '+%Y-%m-%d %H:%M'))
// Gehoert nach firmware/smart_garden/secrets.h (wird von Git ignoriert - NICHT committen).
#pragma once

#define SECRET_WIFI_SSID     "${SSID}"
#define SECRET_WIFI_PASSWORD "${WIFI_PASSWORD}"
#define SECRET_SERVER_URL    "http://${PI_IP}:${PORT}"
#define SECRET_API_KEY       "${API_KEY}"
#define SECRET_DEVICE_ID     "${DEVICE_ID}"
SH
)
SECRETS_H_PATH=""
if [[ -n "${SUDO_USER:-}" && "${SUDO_USER}" != "root" ]]; then
  USER_HOME=$(getent passwd "${SUDO_USER}" | cut -d: -f6)
  if [[ -n "${USER_HOME}" && -d "${USER_HOME}" ]]; then
    SECRETS_H_PATH="${USER_HOME}/smart-garden-secrets.h"
    printf '%s\n' "${SECRETS_H_CONTENT}" > "${SECRETS_H_PATH}"
    chown "${SUDO_USER}:" "${SECRETS_H_PATH}"; chmod 600 "${SECRETS_H_PATH}"
  fi
fi
umask 022

step "5/6 Dienst ${SVC_NAME} starten"
install -m 644 "${DEST}/deploy/smart-garden-api.service" "/etc/systemd/system/${SVC_NAME}.service"
systemctl daemon-reload
systemctl enable "${SVC_NAME}" >/dev/null 2>&1
systemctl restart "${SVC_NAME}"
HEALTH_OK=0
for _ in $(seq 1 30); do
  if curl -fsS "http://127.0.0.1:${PORT}/healthz" >/dev/null 2>&1; then HEALTH_OK=1; break; fi
  sleep 1
done
if [[ "${HEALTH_OK}" -ne 1 ]]; then
  echo "Server antwortet nicht. Letzte Log-Zeilen:" >&2
  journalctl -u "${SVC_NAME}" -n 40 --no-pager >&2 || true
  die "Dienst ${SVC_NAME} laeuft nicht."
fi
curl -fsS -o /dev/null "http://127.0.0.1:${PORT}/" || echo "   WARNUNG: Dashboard (web/dist) wird nicht ausgeliefert."
echo "   Server laeuft (http://127.0.0.1:${PORT}/healthz ok)."

LAN_IP=$(ip -4 -o addr show eth0 2>/dev/null | awk '{print $4}' | cut -d/ -f1 | head -n1)
HOST_NAME=$(hostname)

print_summary() {
  cat <<SUMMARY

==================================================================
  SMART GARDEN - ZUGANGSDATEN
==================================================================
  WLAN fuer ESP32 / Handy / Laptop
    SSID:      ${SSID}
    Passwort:  ${WIFI_PASSWORD}

  Dashboard (im Browser oeffnen)
    im WLAN SmartGarden:   http://${PI_IP}:${PORT}
SUMMARY
  if [[ -n "${LAN_IP}" ]]; then
    echo "    im normalen Netz (LAN): http://${LAN_IP}:${PORT}"
  fi
  echo "    per Name (falls mDNS): http://${HOST_NAME}.local:${PORT}"
  cat <<SUMMARY

  ESP32 (oben in firmware/smart_garden/smart_garden.ino oder in secrets.h)
    const char *CFG_WIFI_SSID     = "${SSID}";
    const char *CFG_WIFI_PASSWORD = "${WIFI_PASSWORD}";
    const char *CFG_SERVER_URL    = "http://${PI_IP}:${PORT}";
    const char *CFG_API_KEY       = "${API_KEY}";
    const char *CFG_DEVICE_ID     = "${DEVICE_ID}";

  Gespeichert auf dem Pi: ${CRED} (nur root)
SUMMARY
  if [[ -n "${SECRETS_H_PATH}" ]]; then
    echo "  Fertige secrets.h:     ${SECRETS_H_PATH}"
  fi
  echo "  Wieder anzeigen:       sudo cat ${CRED}"
  echo "  NICHT ins Git-Repo committen - das Repo ist oeffentlich!"
  echo "=================================================================="
}

step "6/6 WLAN-Hotspot ${SSID}"
if [[ "${WLAN_UPLINK}" -eq 1 ]]; then
  print_summary
  echo
  echo "Hotspot wird jetzt im Hintergrund gestartet - die SSH-Verbindung bricht gleich ab."
  echo "Log danach auf dem Pi: /var/log/smart-garden-hotspot.log"
  SMART_GARDEN_WIFI_PASSWORD="${WIFI_PASSWORD}" setsid nohup bash "${DEST}/deploy/setup_hotspot.sh" \
    </dev/null >/var/log/smart-garden-hotspot.log 2>&1 &
  sleep 2
  exit 0
fi
SMART_GARDEN_WIFI_PASSWORD="${WIFI_PASSWORD}" bash "${DEST}/deploy/setup_hotspot.sh"

print_summary
