#!/usr/bin/env bash
# Smart Garden - one-command demo, no hardware needed.
#
#   git clone https://github.com/Florian221-art/smart_garden.git
#   cd smart_garden
#   bash deploy/demo.sh
#
# What it does: builds the dashboard, creates a throw-away demo database with
# 48 h of simulated history, starts the FastAPI server and a simulated ESP32
# planter (tools/fake_esp.py) that sends live readings in time-lapse, and opens
# the dashboard at http://localhost:8000.
#
# Requirements: Python 3.10+, Node.js 20+ with npm. Works on macOS and Linux
# (on Windows use WSL). Stop with Ctrl+C.
#
# Optional environment variables:
#   PORT=8000          port of the demo server
#   DEMO_SPEED=60      time-lapse factor (simulated seconds per real second)
#   NO_OPEN=1          do not open the browser automatically

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PORT="${PORT:-8000}"
DEMO_SPEED="${DEMO_SPEED:-60}"
DEVICE_ID="esp32-kuebel-01"
DEMO_DIR="$ROOT/.demo"
export SMART_GARDEN_DB_URL="sqlite:///$DEMO_DIR/demo.db"
export SMART_GARDEN_WEB_DIST="$ROOT/web/dist"

say()  { printf '\n\033[1m==> %s\033[0m\n' "$*"; }
fail() { printf '\n\033[31mError: %s\033[0m\n' "$*" >&2; exit 1; }

# --- Prerequisites ----------------------------------------------------------
command -v python3 >/dev/null || fail "python3 not found (3.10 or newer is required)."
command -v node    >/dev/null || fail "node not found (Node.js 20 or newer is required)."
command -v npm     >/dev/null || fail "npm not found."
python3 - <<'PY' || fail "Python 3.10 or newer is required."
import sys; sys.exit(0 if sys.version_info >= (3, 10) else 1)
PY

if command -v lsof >/dev/null && lsof -ti :"$PORT" >/dev/null 2>&1; then
  fail "Port $PORT is already in use. Stop the other server or run: PORT=8010 bash deploy/demo.sh"
fi

# --- Backend ----------------------------------------------------------------
say "Setting up the Python environment (first run only)"
cd "$ROOT/server"
[ -x .venv/bin/python ] || python3 -m venv .venv
./.venv/bin/pip install -q --disable-pip-version-check -r requirements.txt

# --- Dashboard --------------------------------------------------------------
say "Building the dashboard (first run takes a minute)"
cd "$ROOT/web"
if [ -f package-lock.json ]; then npm ci --no-audit --no-fund --loglevel=error
else npm install --no-audit --no-fund --loglevel=error; fi
npm run build --silent

# --- Demo data --------------------------------------------------------------
say "Creating a fresh demo database with 48 h of simulated history"
cd "$ROOT/server"
mkdir -p "$DEMO_DIR"
rm -f "$DEMO_DIR"/demo.db*
./.venv/bin/python -m app.dev_seed "$DEVICE_ID" --hours 48 >/dev/null
API_KEY="$(./.venv/bin/python -m app.cli issue-key "$DEVICE_ID")"

# --- Run --------------------------------------------------------------------
PIDS=()
cleanup() {
  trap - INT TERM EXIT
  printf '\nStopping demo ...\n'
  for pid in "${PIDS[@]:-}"; do [ -n "$pid" ] && kill "$pid" 2>/dev/null || true; done
  wait 2>/dev/null || true
}
trap cleanup INT TERM EXIT

say "Starting the server on http://localhost:$PORT"
./.venv/bin/uvicorn app.main:app --host 127.0.0.1 --port "$PORT" --log-level warning &
PIDS+=("$!")

for _ in $(seq 1 30); do
  curl -fs "http://127.0.0.1:$PORT/healthz" >/dev/null 2>&1 && break
  sleep 1
done
curl -fs "http://127.0.0.1:$PORT/healthz" >/dev/null 2>&1 || fail "The server did not start."

say "Starting the simulated planter (time-lapse x$DEMO_SPEED)"
cd "$ROOT"
python3 tools/fake_esp.py --url "http://127.0.0.1:$PORT" --key "$API_KEY" \
  --device-id "$DEVICE_ID" --interval 2 --speed "$DEMO_SPEED" >/dev/null &
PIDS+=("$!")

URL="http://localhost:$PORT"
cat <<MSG

  Smart Garden demo is running:  $URL
  - Live values come from a simulated ESP32 (soil dries out, pump waters it).
  - Click "Demo" in the dashboard to trigger dry soil, a heat wave or a sensor failure.
  - API documentation:           $URL/docs
  - Stop with Ctrl+C.

MSG

if [ -z "${NO_OPEN:-}" ]; then
  (sleep 1; { command -v open >/dev/null && open "$URL"; } \
            || { command -v xdg-open >/dev/null && xdg-open "$URL"; } \
            || true) >/dev/null 2>&1 &
fi

wait "${PIDS[0]}"
