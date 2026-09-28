"""Verlaufsdaten fuers Dashboard (Diagramme).

GET /api/v1/devices/{id}/readings?from=&to=&bucket=   (plan-webserver.md Abschnitt 1)
GET /api/v1/devices/{id}/config                        (nur lesen; PUT/Admin folgt Phase 3)

Noch ohne Login (kommt Phase 2), genau wie /latest.
"""
from __future__ import annotations

import datetime as dt
import math

from fastapi import APIRouter, Depends, HTTPException, Query
from sqlalchemy.orm import Session

from .. import models
from ..config import DEFAULT_DEVICE_CONFIG
from ..db import get_db
from ..timeutil import as_utc, iso_utc

router = APIRouter(prefix="/api/v1/devices", tags=["history"])

MAX_RANGE = dt.timedelta(days=31)
TARGET_POINTS = 120  # Standard-Aufloesung, wenn kein bucket angegeben ist
MAX_POINTS = 2000
# "Runde" Bucket-Groessen, damit die Achse glatte Zeiten zeigt (15 s ... 6 h)
NICE_BUCKETS = (15, 30, 60, 120, 300, 600, 900, 1800, 3600, 7200, 10800, 21600)

AVG_FIELDS = (
    "soil_moisture_pct",
    "light_pct",
    "air_temp_c",
    "air_humidity_pct",
    "water_level_pct",
    "tank_remaining_ml",
    "rssi_dbm",
)


def _get_device(db: Session, device_id: str) -> models.Device:
    device = db.get(models.Device, device_id)
    if device is None:
        raise HTTPException(status_code=404, detail="device not found")
    return device


def _config_dict(device: models.Device) -> dict:
    return device.config.as_dict() if device.config is not None else dict(DEFAULT_DEVICE_CONFIG)


@router.get("/{device_id}/config")
def get_config(device_id: str, db: Session = Depends(get_db)) -> dict:
    return _config_dict(_get_device(db, device_id))


@router.get("/{device_id}/readings")
def get_readings(
    device_id: str,
    from_: dt.datetime | None = Query(default=None, alias="from"),
    to: dt.datetime | None = Query(default=None),
    bucket: int | None = Query(default=None, ge=1, description="Bucket-Groesse in Sekunden"),
    db: Session = Depends(get_db),
) -> dict:
    device = _get_device(db, device_id)
    cfg = _config_dict(device)

    now = dt.datetime.now(dt.timezone.utc)
    end = as_utc(to) if to else now
    start = as_utc(from_) if from_ else end - dt.timedelta(hours=24)
    if start >= end:
        raise HTTPException(status_code=422, detail="'from' muss vor 'to' liegen")
    if end - start > MAX_RANGE:
        raise HTTPException(status_code=422, detail="Zeitraum max. 31 Tage")

    span_s = (end - start).total_seconds()
    if bucket is None:
        raw = max(1, math.ceil(span_s / TARGET_POINTS))
        bucket = next((b for b in NICE_BUCKETS if b >= raw), NICE_BUCKETS[-1])
    if span_s / bucket > MAX_POINTS:
        raise HTTPException(status_code=422, detail="bucket zu klein fuer diesen Zeitraum")

    # SQLite vergleicht naive Zeitstempel -> Grenzen ohne tzinfo (UTC) uebergeben
    rows = (
        db.query(models.Reading)
        .filter(
            models.Reading.device_id == device_id,
            models.Reading.received_at >= start.replace(tzinfo=None),
            models.Reading.received_at <= end.replace(tzinfo=None),
        )
        .order_by(models.Reading.received_at)
        .all()
    )

    flow = float(cfg["pump_flow_ml_per_s"])
    # Buckets an Vielfachen der Bucket-Groesse ausrichten (z. B. volle 15 min)
    start_epoch = math.floor(start.timestamp() / bucket) * bucket
    buckets: dict[int, dict] = {}
    for r in rows:
        ts = as_utc(r.received_at)
        idx = int((ts.timestamp() - start_epoch) // bucket)
        b = buckets.get(idx)
        if b is None:
            b = buckets[idx] = {
                "n": 0,
                "sums": {f: 0.0 for f in AVG_FIELDS},
                "counts": {f: 0 for f in AVG_FIELDS},
                "pump_on_s": 0.0,
                "auto_water": 0,
                "demo": False,
                "errors": set(),
            }
        b["n"] += 1
        for f in AVG_FIELDS:
            v = getattr(r, f)
            if v is not None:
                b["sums"][f] += float(v)
                b["counts"][f] += 1
        b["pump_on_s"] += r.pump_on_s_since_last or 0.0
        if r.auto_water_triggered:
            b["auto_water"] += 1
        if r.demo_overrides:
            b["demo"] = True
        b["errors"].update(r.errors or [])

    points = []
    for idx in sorted(buckets):
        b = buckets[idx]
        t = dt.datetime.fromtimestamp(start_epoch + idx * bucket, tz=dt.timezone.utc)
        p = {"t": iso_utc(t), "n": b["n"]}
        for f in AVG_FIELDS:
            c = b["counts"][f]
            p[f] = round(b["sums"][f] / c, 2) if c else None
        p["pump_on_s"] = round(b["pump_on_s"], 2)
        p["water_ml"] = round(b["pump_on_s"] * flow, 1)
        p["auto_water_count"] = b["auto_water"]
        p["demo"] = b["demo"]
        p["errors"] = sorted(b["errors"])
        points.append(p)

    return {
        "device_id": device_id,
        "from": iso_utc(start),
        "to": iso_utc(end),
        "bucket_s": bucket,
        "config": cfg,
        "points": points,
    }
