"""Demo-Modus + Geraetebefehle: gemeinsame Logik (api-contract.md Abschnitte 2 und 4)."""
from __future__ import annotations

import datetime as dt
import logging
import math

from sqlalchemy.orm import Session

from .models import DemoState
from .timeutil import as_utc

log = logging.getLogger("smart_garden.demo")

MAX_DEMO_S = 600  # Vertrag: expires_in_s max. 600

# Erlaubte Overrides laut Vertrag mit sinnvollen Wertebereichen
OVERRIDE_RANGES: dict[str, tuple[float, float]] = {
    "soil_moisture_pct": (0, 100),
    "light_pct": (0, 100),
    "air_temp_c": (-20, 60),
    "air_humidity_pct": (0, 100),
    "water_level_pct": (0, 100),
}

# Fehlercodes, die die Firmware erzwingen kann (demo.cpp: dht*/soil*/light*)
FORCEABLE_ERRORS = ("dht_read_failed", "soil_out_of_range", "light_read_failed")


def _now() -> dt.datetime:
    return dt.datetime.now(dt.timezone.utc)


def get_active(db: Session, device_id: str) -> DemoState | None:
    """Aktiven Demo-Zustand liefern; abgelaufenen dabei loeschen."""
    state = db.get(DemoState, device_id)
    if state is None:
        return None
    if as_utc(state.expires_at) <= _now():
        db.delete(state)
        db.commit()
        log.info("demo expired device=%s", device_id)
        return None
    return state


def remaining_s(state: DemoState) -> int:
    return max(0, math.ceil((as_utc(state.expires_at) - _now()).total_seconds()))


def esp_payload(state: DemoState | None) -> dict | None:
    """Objekt `demo` fuer die Antwort an den ESP (oder None = Demo aus)."""
    if state is None:
        return None
    return {
        "expires_in_s": min(MAX_DEMO_S, remaining_s(state)),
        "overrides": dict(state.overrides or {}),
        "force_errors": list(state.force_errors or []),
    }
