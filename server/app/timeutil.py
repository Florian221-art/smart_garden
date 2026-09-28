"""Zeit-Helfer. SQLite speichert Zeitstempel ohne Zeitzone -> wir behandeln sie als UTC."""
from __future__ import annotations

import datetime as dt


def as_utc(ts: dt.datetime) -> dt.datetime:
    if ts.tzinfo is None:
        return ts.replace(tzinfo=dt.timezone.utc)
    return ts.astimezone(dt.timezone.utc)


def iso_utc(ts: dt.datetime) -> str:
    """ISO-8601 mit 'Z', damit der Browser die Zeit korrekt in Ortszeit umrechnet."""
    return as_utc(ts).isoformat().replace("+00:00", "Z")
