"""Steuerung fuers Dashboard: Demo-Modus und Geraetebefehle.

GET/PUT/DELETE /api/v1/devices/{id}/demo       (plan-webserver.md Abschnitt 5)
GET/POST       /api/v1/devices/{id}/commands   (gießen, Tank aufgefuellt, identify, Summer)

ACHTUNG: Laut Plan nur fuer Admins. Login/Rollen gibt es noch nicht (Phase 2) -
bis dahin kann jeder im Hotspot-WLAN diese Endpunkte nutzen. Die Pumpe bleibt
trotzdem geschuetzt: Der ESP begrenzt Laufzeit, Pause und Tageslimit hart.
Jede Aenderung wird bis zum echten Zugriffsprotokoll ins Server-Log geschrieben.
"""
from __future__ import annotations

import datetime as dt
import logging

from fastapi import APIRouter, Depends, HTTPException, Request
from sqlalchemy.orm import Session

from .. import models
from ..config import DEFAULT_DEVICE_CONFIG
from ..db import get_db
from ..demo import get_active, remaining_s
from ..schemas import CommandIn, CommandsOut, DemoIn, DemoOut
from ..timeutil import iso_utc

router = APIRouter(prefix="/api/v1/devices", tags=["control"])
log = logging.getLogger("smart_garden.control")


def _device(db: Session, device_id: str) -> models.Device:
    device = db.get(models.Device, device_id)
    if device is None:
        raise HTTPException(status_code=404, detail="device not found")
    return device


def _client(request: Request) -> str:
    return request.client.host if request.client else "?"


def _demo_out(state: models.DemoState | None) -> DemoOut:
    if state is None:
        return DemoOut(active=False)
    return DemoOut(
        active=True,
        overrides=dict(state.overrides or {}),
        force_errors=list(state.force_errors or []),
        scenario=state.scenario,
        started_at=iso_utc(state.started_at),
        expires_at=iso_utc(state.expires_at),
        remaining_s=remaining_s(state),
    )


@router.get("/{device_id}/demo", response_model=DemoOut)
def get_demo(device_id: str, db: Session = Depends(get_db)) -> DemoOut:
    _device(db, device_id)
    return _demo_out(get_active(db, device_id))


@router.put("/{device_id}/demo", response_model=DemoOut)
def set_demo(device_id: str, body: DemoIn, request: Request, db: Session = Depends(get_db)) -> DemoOut:
    """Demo-Modus setzen bzw. ersetzen. Der ESP uebernimmt ihn mit seiner naechsten Meldung."""
    _device(db, device_id)
    now = dt.datetime.now(dt.timezone.utc)
    state = db.get(models.DemoState, device_id)
    if state is None:
        state = models.DemoState(device_id=device_id)
        db.add(state)
    state.overrides = body.overrides
    state.force_errors = body.force_errors
    state.scenario = body.scenario
    state.started_at = now
    state.expires_at = now + dt.timedelta(seconds=body.duration_s)
    db.commit()
    db.refresh(state)
    log.info(
        "demo set device=%s ip=%s scenario=%s overrides=%s errors=%s duration=%ss",
        device_id, _client(request), body.scenario, body.overrides, body.force_errors, body.duration_s,
    )
    return _demo_out(state)


@router.delete("/{device_id}/demo", response_model=DemoOut)
def clear_demo(device_id: str, request: Request, db: Session = Depends(get_db)) -> DemoOut:
    """Demo sofort beenden - der ESP bekommt bei der naechsten Meldung demo: null."""
    _device(db, device_id)
    state = db.get(models.DemoState, device_id)
    if state is not None:
        db.delete(state)
        db.commit()
    log.info("demo cleared device=%s ip=%s", device_id, _client(request))
    return DemoOut(active=False)


def _pending(db: Session, device_id: str) -> models.PendingCommand:
    pending = db.get(models.PendingCommand, device_id)
    if pending is None:
        pending = models.PendingCommand(
            device_id=device_id, pump_run_s=0, buzzer=False, identify=False, tank_refilled=False
        )
        db.add(pending)
    return pending


def _commands_out(p: models.PendingCommand | None) -> CommandsOut:
    if p is None:
        return CommandsOut()
    return CommandsOut(pump_run_s=p.pump_run_s, buzzer=p.buzzer, identify=p.identify, tank_refilled=p.tank_refilled)


@router.get("/{device_id}/commands", response_model=CommandsOut)
def get_commands(device_id: str, db: Session = Depends(get_db)) -> CommandsOut:
    """Noch nicht ausgelieferte Befehle (werden mit der naechsten ESP-Meldung zugestellt)."""
    _device(db, device_id)
    return _commands_out(db.get(models.PendingCommand, device_id))


@router.post("/{device_id}/commands", response_model=CommandsOut)
def queue_command(device_id: str, body: CommandIn, request: Request, db: Session = Depends(get_db)) -> CommandsOut:
    device = _device(db, device_id)
    cfg = device.config.as_dict() if device.config is not None else DEFAULT_DEVICE_CONFIG
    pending = _pending(db, device_id)
    if body.pump_run_s:
        # Server begrenzt schon auf max_pump_s_per_run; der ESP begrenzt zusaetzlich hart
        pending.pump_run_s = min(float(body.pump_run_s), float(cfg["max_pump_s_per_run"]))
    pending.tank_refilled = pending.tank_refilled or body.tank_refilled
    pending.identify = pending.identify or body.identify
    pending.buzzer = pending.buzzer or body.buzzer
    db.commit()
    log.info("command queued device=%s ip=%s %s", device_id, _client(request), body.model_dump(exclude_defaults=True))
    return _commands_out(pending)
