"""Einfacher In-Memory Rate-Limiter: max. 1 Request / RATE_LIMIT_SECONDS pro Geraet.

Phase 1: ein Prozess, In-Memory reicht (siehe api-contract.md Abschnitt 2,
Fehlercode 429). Bei Mehrprozess-Deployment (Phase 3+) muesste das nach
Redis/SQLite wandern.
"""
from __future__ import annotations

import threading
import time

from .config import RATE_LIMIT_SECONDS

_lock = threading.Lock()
_last_seen: dict[str, float] = {}


def check_rate_limit(device_id: str) -> bool:
    """True = ok, False = geblockt (429)."""
    now = time.monotonic()
    with _lock:
        last = _last_seen.get(device_id)
        if last is not None and (now - last) < RATE_LIMIT_SECONDS:
            return False
        _last_seen[device_id] = now
        return True
