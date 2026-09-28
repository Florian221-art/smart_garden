#!/usr/bin/env bash
# Richtet auf dem Raspberry Pi 5 einen eigenen WLAN-Hotspot ein (wlan0),
# unabhaengig vom Hackathon-WLAN. Internet fuer den Pi kommt ueber Ethernet.
#
# Siehe .claude/api-contract.md Abschnitt 1 und plan-webserver.md Aufgabe 0.
#
# Nutzung (auf dem Pi, als root/sudo):
#   sudo ./deploy/setup_hotspot.sh <WLAN-Passwort>
#
# WICHTIG: Das Passwort NICHT ins Repo committen. Dieses Skript nimmt es nur
# als Kommandozeilenargument entgegen bzw. liest es interaktiv ein.

set -euo pipefail

SSID="SmartGarden"
IFACE="wlan0"
CON_NAME="SmartGardenHotspot"
PI_IP="10.42.0.1"   # Standard-IP von NetworkManager-Hotspots

if [[ "${EUID}" -ne 0 ]]; then
  echo "Bitte mit sudo/als root ausfuehren." >&2
  exit 1
fi

if ! command -v nmcli >/dev/null 2>&1; then
  echo "nmcli (NetworkManager) nicht gefunden. Auf Raspberry Pi OS Bookworm ist" >&2
  echo "NetworkManager Standard; falls nicht: 'sudo apt install network-manager'." >&2
  exit 1
fi

WIFI_PASSWORD="${1:-}"
if [[ -z "${WIFI_PASSWORD}" ]]; then
  read -r -s -p "WLAN-Passwort fuer SSID '${SSID}' (min. 8 Zeichen, WPA2): " WIFI_PASSWORD
  echo
fi
if [[ "${#WIFI_PASSWORD}" -lt 8 ]]; then
  echo "Passwort muss mindestens 8 Zeichen lang sein (WPA2)." >&2
  exit 1
fi

echo "-> Entferne evtl. vorhandene Verbindung '${CON_NAME}' (idempotent)..."
nmcli connection delete "${CON_NAME}" >/dev/null 2>&1 || true

echo "-> Erzeuge Hotspot '${SSID}' auf ${IFACE} (Pi-IP ${PI_IP})..."
nmcli dev wifi hotspot ifname "${IFACE}" con-name "${CON_NAME}" ssid "${SSID}" password "${WIFI_PASSWORD}"

echo "-> Hotspot startet automatisch beim Boot..."
nmcli connection modify "${CON_NAME}" connection.autoconnect yes
nmcli connection modify "${CON_NAME}" connection.autoconnect-priority 100

echo "-> ufw-Regeln (falls ufw installiert): 22, 80, 443, 8000 (Phase 1)..."
if command -v ufw >/dev/null 2>&1; then
  ufw allow 22/tcp || true
  ufw allow 80/tcp || true
  ufw allow 443/tcp || true
  ufw allow 8000/tcp || true
else
  echo "   ufw nicht installiert - siehe deploy/README.md."
fi

echo
echo "Fertig. SSID '${SSID}' ist aktiv, Pi erreichbar unter http://${PI_IP}:8000"
echo "Merke dir das Passwort fuer secrets.h auf dem ESP32 (nicht committen!)."
