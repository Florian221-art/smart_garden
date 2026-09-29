"""Zentrale Konfiguration/Defaults fuer den Smart-Garden-Server.

Phase 1: Werte sind hart codiert / per .env ueberschreibbar. Ab Phase 3
werden geraetespezifische Werte in device_config editierbar (Admin-UI).
"""
from __future__ import annotations

import os
from pathlib import Path

BASE_DIR = Path(__file__).resolve().parent.parent

# SQLite-Datei liegt in server/ (siehe .gitignore, wird nicht committet)
DATABASE_URL = os.environ.get(
    "SMART_GARDEN_DB_URL", f"sqlite:///{BASE_DIR / 'smart_garden.db'}"
)

# Fertig gebautes Dashboard (web/dist, erzeugt mit `npm run build`). Existiert der
# Ordner, liefert FastAPI es unter / aus - dann braucht es auf dem Pi (und fuers
# Handy im Hotspot-WLAN) keinen separaten Webserver. /api bleibt davon unberuehrt.
WEB_DIST = Path(os.environ.get("SMART_GARDEN_WEB_DIST", BASE_DIR.parent / "web" / "dist"))

# Rate-Limit laut api-contract.md Abschnitt 2: max. 1 Request / 2s pro Geraet
RATE_LIMIT_SECONDS = float(os.environ.get("SMART_GARDEN_RATE_LIMIT_S", "2.0"))

# Hoechstens so lange darf EIN Pumpenstoss dauern (Sekunden) - egal ob Auto-Giessen, Demo
# oder Knopf "Jetzt giessen". Die Pumpe ist stark: laengere Stoesse setzen alles unter Wasser.
# Wird bei jeder Config an den ESP und bei jedem Giess-Befehl hart erzwungen.
MAX_PUMP_S_PER_RUN = 0.5

# Default-Config, die dem ESP bei jeder Antwort mitgeschickt wird
# (Werte 1:1 aus api-contract.md Abschnitt 2 / Beispielantwort).
DEFAULT_DEVICE_CONFIG: dict = {
    "interval_s": 15,
    "moisture_min_pct": 30,
    "moisture_target_pct": 55,
    "auto_water": True,
    # 0,5 s statt 5 s: die Pumpe ist stark, kurze Stoesse reichen (Issue #32, Firmware 0.3.8)
    "max_pump_s_per_run": MAX_PUMP_S_PER_RUN,
    "pump_cooldown_s": 300,
    "max_pump_s_per_day": 60,
    "buzzer_enabled": True,
    "tank_capacity_ml": 1500,
    "pump_flow_ml_per_s": 20,
    "tank_low_pct": 20,
}
