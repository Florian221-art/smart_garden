#!/usr/bin/env bash
# Bringt Smart Garden vom Laptop (Mac/Linux) auf den Raspberry Pi - ein Befehl.
#
# Nutzung (im Repo-Ordner auf dem Laptop):
#   ./deploy/deploy_to_pi.sh <benutzer>@<pi-adresse>
#   z.B. ./deploy/deploy_to_pi.sh pi@raspberrypi.local
#
# Ablauf:
#   1. Dashboard bauen (web/ -> web/dist/, braucht Node.js auf dem Laptop)
#   2. server/, web/dist/, deploy/, tools/ per SSH auf den Pi kopieren (~/smart-garden-upload)
#   3. Auf dem Pi deploy/install_pi.sh mit sudo starten (fragt evtl. nach dem Pi-Passwort)
#   4. Fertige secrets.h fuer den ESP32 vom Pi holen -> firmware/smart_garden/secrets.h
#      (wird von Git ignoriert - NICHT committen)
#
# Voraussetzungen: SSH auf dem Pi aktiviert, Pi per LAN-Kabel am Netz
# (wlan0 wird fuer den Hotspot gebraucht), Raspberry Pi OS Bookworm oder neuer.
# Beliebig oft wiederholbar - Datenbank, API-Key und WLAN-Passwort bleiben erhalten.

set -euo pipefail

TARGET="${1:-}"
if [[ -z "${TARGET}" ]]; then
  echo "Nutzung: $0 <benutzer>@<pi-adresse>   (z.B. pi@raspberrypi.local)" >&2
  exit 1
fi

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${REPO}"

# Eine SSH-Verbindung fuer alle Schritte -> Passwort nur einmal eingeben
# bewusst /tmp statt $TMPDIR: macOS-$TMPDIR ist so lang, dass der Socket-Pfad das 104-Zeichen-Limit sprengt
SOCK_DIR="$(mktemp -d /tmp/sg-ssh.XXXXXX)"
SSH_OPTS=(-o ControlMaster=auto -o "ControlPath=${SOCK_DIR}/%C" -o ControlPersist=300 -o ConnectTimeout=10)
cleanup() {
  ssh "${SSH_OPTS[@]}" -O exit "${TARGET}" >/dev/null 2>&1 || true
  rm -rf "${SOCK_DIR}"
}
trap cleanup EXIT

echo "==> Verbindung zu ${TARGET} pruefen..."
if ! ssh "${SSH_OPTS[@]}" "${TARGET}" true; then
  echo "FEHLER: Keine SSH-Verbindung zu ${TARGET}." >&2
  echo "  - Ist der Pi an und im selben Netz wie der Laptop?" >&2
  echo "  - SSH aktiviert? (Raspberry Pi Imager -> Einstellungen -> SSH, oder 'sudo raspi-config')" >&2
  echo "  - Adresse testen: ping raspberrypi.local  (oder IP-Adresse aus dem Router nehmen)" >&2
  exit 1
fi

echo "==> Dashboard bauen (web/dist)..."
command -v npm >/dev/null 2>&1 || { echo "FEHLER: npm/Node.js fehlt auf dem Laptop." >&2; exit 1; }
(
  cd web
  # immer ausfuehren: holt neue Pakete (z. B. nach git pull), ist sonst nach Sekunden fertig
  npm install --no-audit --no-fund
  npm run build
)
[[ -f web/dist/index.html ]] || { echo "FEHLER: Build hat kein web/dist/index.html erzeugt." >&2; exit 1; }

echo "==> Dateien auf den Pi kopieren..."
PARTS=(server web/dist deploy)
[[ -d tools ]] && PARTS+=(tools)
COPYFILE_DISABLE=1 tar -czf - \
  --exclude='.venv' --exclude='__pycache__' --exclude='*.db' --exclude='*.db-*' \
  --exclude='* 2' --exclude='* 2.*' --exclude='.DS_Store' --exclude='._*' \
  "${PARTS[@]}" \
  | ssh "${SSH_OPTS[@]}" "${TARGET}" \
      'rm -rf ~/smart-garden-upload && mkdir -p ~/smart-garden-upload && tar -xzf - -C ~/smart-garden-upload --warning=no-unknown-keyword'

echo "==> Installation auf dem Pi starten (sudo fragt evtl. nach dem Passwort des Pi-Benutzers)..."
set +e
ssh "${SSH_OPTS[@]}" -t "${TARGET}" "sudo ${FORCE_WLAN:+FORCE_WLAN=1 }bash ~/smart-garden-upload/deploy/install_pi.sh"
RC=$?
set -e
if [[ ${RC} -ne 0 ]]; then
  if [[ -n "${FORCE_WLAN:-}" ]]; then
    echo "Verbindung beendet (bei FORCE_WLAN erwartet - der Pi ist jetzt im Hotspot-Modus)."
    exit 0
  fi
  echo "FEHLER: Installation auf dem Pi fehlgeschlagen (Exit-Code ${RC}), siehe Ausgabe oben." >&2
  exit "${RC}"
fi

echo
echo "==> secrets.h fuer den ESP32 holen..."
TMP_SECRETS="$(mktemp)"
if ssh "${SSH_OPTS[@]}" "${TARGET}" 'cat ~/smart-garden-secrets.h' > "${TMP_SECRETS}" 2>/dev/null && [[ -s "${TMP_SECRETS}" ]]; then
  DEST_SECRETS="firmware/smart_garden/secrets.h"
  if [[ -d firmware/smart_garden ]]; then
    if [[ -f "${DEST_SECRETS}" ]] && ! cmp -s "${TMP_SECRETS}" "${DEST_SECRETS}"; then
      cp "${DEST_SECRETS}" "${DEST_SECRETS}.vorher"
      echo "   Alte secrets.h gesichert als ${DEST_SECRETS}.vorher"
    fi
    cp "${TMP_SECRETS}" "${DEST_SECRETS}"
    chmod 600 "${DEST_SECRETS}"
    echo "   Gespeichert: ${DEST_SECRETS} (von Git ignoriert - NICHT committen)"
    echo "   -> In der Arduino IDE den Sketch neu oeffnen und auf den ESP32 hochladen."
  fi
else
  echo "   Konnte secrets.h nicht holen - Zugangsdaten stehen oben in der Ausgabe."
fi
rm -f "${TMP_SECRETS}"

echo
echo "Fertig. Dashboard: im WLAN 'SmartGarden' -> http://10.42.0.1:8000"
