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

# Rate-Limit laut api-contract.md Abschnitt 2: max. 1 Request / 2s pro Geraet
RATE_LIMIT_SECONDS = float(os.environ.get("SMART_GARDEN_RATE_LIMIT_S", "2.0"))

# Default-Config, die dem ESP bei jeder Antwort mitgeschickt wird
# (Werte 1:1 aus api-contract.md Abschnitt 2 / Beispielantwort).
DEFAULT_DEVICE_CONFIG: dict = {
    "interval_s": 15,
    "moisture_min_pct": 30,
    "moisture_target_pct": 55,
    "auto_water": True,
    "max_pump_s_per_run": 5,
    "pump_cooldown_s": 300,
    "max_pump_s_per_day": 60,
    "buzzer_enabled": True,
    "tank_capacity_ml": 1500,
    "pump_flow_ml_per_s": 20,
    "tank_low_pct": 20,
}
