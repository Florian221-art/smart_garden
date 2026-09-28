"""Endpunkte rund um Messwerte: POST /api/v1/readings, GET .../latest.

Vertrag: .claude/api-contract.md Abschnitt 2 (v1.2).
"""
from __future__ import annotations

from fastapi import APIRouter, Depends, Header, HTTPException, Request
from fastapi.responses import JSONResponse
from sqlalchemy.orm import Session

from .. import models
from ..auth import verify_api_key
from ..config import DEFAULT_DEVICE_CONFIG
from ..db import get_db
from ..ratelimit import check_rate_limit
from ..schemas import (
    CommandsOut,
    ConfigOut,
    LatestReadingOut,
    ReadingIn,
    ReadingResponse,
)

router = APIRouter(prefix="/api/v1", tags=["readings"])


def _err(status_code: int, error: str, detail: str | None = None) -> JSONResponse:
    body = {"ok": False, "error": error}
    if detail is not None:
        body["detail"] = detail
    return JSONResponse(status_code=status_code, content=body)


@router.post("/readings", response_model=ReadingResponse)
def post_reading(
    payload: ReadingIn,
    request: Request,
    db: Session = Depends(get_db),
    x_api_key: str | None = Header(default=None),
):
    # --- Auth: Header vorhanden + passt zu device_id ---
    if not x_api_key:
        return _err(401, "unauthorized")

    device = db.get(models.Device, payload.device_id)
    if device is None or not verify_api_key(x_api_key, device.api_key_hash):
        return _err(401, "unauthorized")

    # --- Rate-Limit: max. 1 Request / 2s pro Geraet ---
    if not check_rate_limit(payload.device_id):
        return _err(429, "rate_limited")

    # --- Messwert speichern (received_at setzt der Server, siehe Vertrag) ---
    reading = models.Reading(
        device_id=payload.device_id,
        fw_version=payload.fw_version,
        seq=payload.seq,
        uptime_s=payload.uptime_s,
        rssi_dbm=payload.rssi_dbm,
        soil_moisture_pct=payload.soil_moisture_pct,
        soil_moisture_raw=payload.soil_moisture_raw,
        light_pct=payload.light_pct,
        light_raw=payload.light_raw,
        air_temp_c=payload.air_temp_c,
        air_humidity_pct=payload.air_humidity_pct,
        tank_remaining_ml=payload.tank_remaining_ml,
        water_level_pct=payload.water_level_pct,
        pump_on_s_since_last=payload.pump_on_s_since_last,
        pump_running=payload.pump_running,
        auto_water_triggered=payload.auto_water_triggered,
        errors=payload.errors,
        demo_overrides=payload.demo_overrides,
    )
    db.add(reading)

    # --- Pending Command holen + als erledigt markieren (genau einmal) ---
    pending = db.get(models.PendingCommand, payload.device_id)
    if pending is None:
        pending = models.PendingCommand(
            device_id=payload.device_id,
            pump_run_s=0,
            buzzer=False,
            identify=False,
            tank_refilled=False,
        )
        db.add(pending)
    commands = pending.as_dict_and_clear()

    # --- Config zusammenstellen ---
    cfg = device.config
    config_dict = cfg.as_dict() if cfg is not None else dict(DEFAULT_DEVICE_CONFIG)

    db.commit()

    return ReadingResponse(
        ok=True,
        commands=CommandsOut(**commands),
        config=ConfigOut(**config_dict),
        demo=None,  # Demo-Modus folgt in Phase 2
    )


@router.get("/devices/{device_id}/latest", response_model=LatestReadingOut)
def get_latest_reading(device_id: str, db: Session = Depends(get_db)):
    device = db.get(models.Device, device_id)
    if device is None:
        raise HTTPException(status_code=404, detail="device not found")

    reading = (
        db.query(models.Reading)
        .filter(models.Reading.device_id == device_id)
        .order_by(models.Reading.received_at.desc())
        .first()
    )
    if reading is None:
        raise HTTPException(status_code=404, detail="no readings yet")

    return LatestReadingOut(
        device_id=reading.device_id,
        received_at=reading.received_at.isoformat(),
        fw_version=reading.fw_version,
        seq=reading.seq,
        uptime_s=reading.uptime_s,
        rssi_dbm=reading.rssi_dbm,
        soil_moisture_pct=reading.soil_moisture_pct,
        soil_moisture_raw=reading.soil_moisture_raw,
        light_pct=reading.light_pct,
        light_raw=reading.light_raw,
        air_temp_c=reading.air_temp_c,
        air_humidity_pct=reading.air_humidity_pct,
        tank_remaining_ml=reading.tank_remaining_ml,
        water_level_pct=reading.water_level_pct,
        pump_on_s_since_last=reading.pump_on_s_since_last,
        pump_running=reading.pump_running,
        auto_water_triggered=reading.auto_water_triggered,
        errors=reading.errors,
        demo_overrides=reading.demo_overrides,
    )
