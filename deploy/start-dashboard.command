#!/bin/bash
# Smart Garden - alles fuer die Entwicklung auf dem Mac starten.
# Doppelklick im Finder (oeffnet ein Terminal) oder im Terminal: ./deploy/start-dashboard.command
# Beenden: in diesem Fenster Ctrl + C (stoppt Backend und Frontend).

cd "$(dirname "$0")/.." || exit 1
ROOT="$(pwd)"
LOG="${TMPDIR:-/tmp}/smart-garden-backend.log"

echo "=== Smart Garden startet ==="

# Evtl. noch laufende alte Server beenden (Port 8000 = Backend, 5173 = Frontend)
for port in 8000 5173; do
  pids=$(lsof -ti :"$port" 2>/dev/null)
  if [ -n "$pids" ]; then echo "Beende alten Server auf Port $port"; kill $pids 2>/dev/null; fi
done
sleep 1

# --- Backend ---
cd "$ROOT/server" || exit 1
if [ ! -x .venv/bin/python ]; then
  echo "Lege Python-Umgebung an (einmalig) ..."
  python3 -m venv .venv || { echo "python3 fehlt"; read -r; exit 1; }
fi
./.venv/bin/pip install -q -r requirements.txt || { echo "pip install fehlgeschlagen"; read -r; exit 1; }

# Datenbank leer? Dann Geraet + 48 h Testdaten einspielen
DEVICES=$(./.venv/bin/python -c "
import sqlite3
try:
    print(sqlite3.connect('smart_garden.db').execute('select count(*) from devices').fetchone()[0])
except Exception:
    print(0)
" 2>/dev/null)
if [ "$DEVICES" = "0" ]; then
  echo "Datenbank ist leer - spiele Testdaten ein ..."
  ./.venv/bin/python -m app.dev_seed esp32-kuebel-01 --hours 48
  echo "(Den X-API-Key oben brauchst du nur fuer tools/fake_esp.py)"
fi

./.venv/bin/uvicorn app.main:app --reload --reload-dir app > "$LOG" 2>&1 &
BACKEND=$!
trap 'echo; echo "Beende Server ..."; kill $BACKEND 2>/dev/null; exit 0' INT TERM EXIT

for _ in 1 2 3 4 5 6 7 8 9 10; do
  curl -s http://127.0.0.1:8000/healthz >/dev/null 2>&1 && break
  sleep 1
done
if ! curl -s http://127.0.0.1:8000/healthz >/dev/null 2>&1; then
  echo "Backend startet nicht - letzte Zeilen aus $LOG:"
  tail -20 "$LOG"
  read -r
  exit 1
fi
echo "Backend laeuft (Log: $LOG)"

# --- Frontend ---
cd "$ROOT/web" || exit 1
if [ ! -d node_modules ]; then
  echo "Installiere Frontend-Pakete (einmalig) ..."
  npm install || { echo "npm install fehlgeschlagen"; read -r; exit 1; }
fi

IP=$(ipconfig getifaddr en0 2>/dev/null || ipconfig getifaddr en1 2>/dev/null)
echo
echo "======================================================"
echo "  Mac:    http://localhost:5173"
[ -n "$IP" ] && echo "  Handy:  http://$IP:5173   (gleiches WLAN)"
echo "  Beenden: Ctrl + C in diesem Fenster"
echo "======================================================"
echo
(sleep 3 && open "http://localhost:5173") &
npm run dev:mobil
