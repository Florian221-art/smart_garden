#!/usr/bin/env bash
# Richtet auf dem Raspberry Pi 5 einen eigenen WLAN-Hotspot ein (wlan0),
# unabhaengig vom Hackathon-WLAN. Internet fuer den Pi kommt ueber Ethernet.
#
# Siehe .claude/api-contract.md Abschnitt 1 und plan-webserver.md Aufgabe 0.
#
# Nutzung (auf dem Pi, als root/sudo):
#   sudo ./deploy/setup_hotspot.sh <WLAN-Passwort>
#   (oder ohne Argument: Passwort wird interaktiv abgefragt,
#    oder per Umgebungsvariable SMART_GARDEN_WIFI_PASSWORD)
#
# Wird normalerweise automatisch von deploy/install_pi.sh aufgerufen.
#
# WICHTIG: Das Passwort NICHT ins Repo committen. Dieses Skript nimmt es nur
# als Argument/Umgebungsvariable entgegen bzw. liest es interaktiv ein.
#
# ESP32-Kompatibilitaet: Der ESP32 kann nur 2,4 GHz und kommt mit WPA3/PMF
# nicht immer zurecht. Deshalb wird der Hotspot fest auf 2,4 GHz (band bg,
# Kanal 6), reines WPA2-PSK (RSN/CCMP) und PMF aus gestellt.

set -euo pipefail

SSID="SmartGarden"
IFACE="wlan0"
CON_NAME="SmartGardenHotspot"
PI_IP="10.42.0.1"   # Standard-IP von NetworkManager-Hotspots (Modus "shared")
CHANNEL="6"
COUNTRY="${SMART_GARDEN_WIFI_COUNTRY:-DE}"

if [[ "${EUID}" -ne 0 ]]; then
  echo "Bitte mit sudo/als root ausfuehren." >&2
  exit 1
fi

if ! command -v nmcli >/dev/null 2>&1; then
  echo "nmcli (NetworkManager) nicht gefunden. Auf Raspberry Pi OS Bookworm ist" >&2
  echo "NetworkManager Standard; falls nicht: 'sudo apt install network-manager'." >&2
  exit 1
fi

WIFI_PASSWORD="${1:-${SMART_GARDEN_WIFI_PASSWORD:-}}"
if [[ -z "${WIFI_PASSWORD}" ]]; then
  read -r -s -p "WLAN-Passwort fuer SSID '${SSID}' (min. 8 Zeichen, WPA2): " WIFI_PASSWORD
  echo
fi
if [[ "${#WIFI_PASSWORD}" -lt 8 || "${#WIFI_PASSWORD}" -gt 63 ]]; then
  echo "Passwort muss 8 bis 63 Zeichen lang sein (WPA2)." >&2
  exit 1
fi

echo "-> WLAN-Land auf ${COUNTRY} setzen und WLAN entsperren..."
if command -v raspi-config >/dev/null 2>&1; then
  raspi-config nonint do_wifi_country "${COUNTRY}" || true
elif command -v iw >/dev/null 2>&1; then
  iw reg set "${COUNTRY}" || true
fi
if command -v rfkill >/dev/null 2>&1; then
  rfkill unblock wifi || true
fi
nmcli radio wifi on || true

echo "-> Entferne evtl. vorhandene Verbindung '${CON_NAME}' (idempotent)..."
nmcli connection delete "${CON_NAME}" >/dev/null 2>&1 || true

echo "-> Erzeuge Hotspot '${SSID}' auf ${IFACE} (Pi-IP ${PI_IP})..."
nmcli connection add type wifi ifname "${IFACE}" con-name "${CON_NAME}" \
  autoconnect yes ssid "${SSID}" >/dev/null
nmcli connection modify "${CON_NAME}" \
  connection.autoconnect-priority 100 \
  802-11-wireless.mode ap \
  802-11-wireless.band bg \
  802-11-wireless.channel "${CHANNEL}" \
  ipv4.method shared \
  ipv4.addresses "${PI_IP}/24" \
  ipv6.method disabled \
  wifi-sec.key-mgmt wpa-psk \
  wifi-sec.proto rsn \
  wifi-sec.pairwise ccmp \
  wifi-sec.group ccmp \
  wifi-sec.pmf disable \
  wifi-sec.psk "${WIFI_PASSWORD}"

echo "-> Hotspot starten..."
nmcli connection up "${CON_NAME}"

echo "-> ufw-Regeln (falls ufw installiert): 22, 80, 443, 8000 (Phase 1)..."
if command -v ufw >/dev/null 2>&1; then
  ufw allow 22/tcp || true
  ufw allow 80/tcp || true
  ufw allow 443/tcp || true
  ufw allow 8000/tcp || true
  # DHCP/DNS fuer Geraete im Hotspot (NetworkManager-dnsmasq)
  ufw allow in on "${IFACE}" to any port 67 proto udp || true
  ufw allow in on "${IFACE}" to any port 53 || true
else
  echo "   ufw nicht installiert - siehe deploy/README.md."
fi

echo
echo "Fertig. SSID '${SSID}' ist aktiv (2,4 GHz, Kanal ${CHANNEL}, WPA2)."
echo "Pi erreichbar unter http://${PI_IP}:8000"
