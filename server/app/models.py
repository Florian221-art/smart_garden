"""SQLAlchemy-Modelle.

Tabellen laut plan-webserver.md Abschnitt 2. Phase 1 legt devices, readings,
device_config und commands an (alles was fuer POST /api/v1/readings noetig
ist). alerts/users/access_log/demo_state/insights_cache folgen in
Phase 2/3.
"""
from __future__ import annotations

import datetime as dt

from sqlalchemy import (
    JSON,
    Boolean,
    DateTime,
    Float,
    ForeignKey,
    Integer,
    String,
)
from sqlalchemy.orm import Mapped, mapped_column, relationship

from .config import DEFAULT_DEVICE_CONFIG
from .db import Base


def _utcnow() -> dt.datetime:
    return dt.datetime.now(dt.timezone.utc)


class Device(Base):
    __tablename__ = "devices"

    id: Mapped[str] = mapped_column(String(32), primary_key=True)  # device_id
    api_key_hash: Mapped[str] = mapped_column(String(255), nullable=False)
    created_at: Mapped[dt.datetime] = mapped_column(DateTime(timezone=True), default=_utcnow)
    label: Mapped[str | None] = mapped_column(String(64), nullable=True)

    config: Mapped["DeviceConfig"] = relationship(
        back_populates="device", uselist=False, cascade="all, delete-orphan"
    )
    readings: Mapped[list["Reading"]] = relationship(
        back_populates="device", cascade="all, delete-orphan"
    )


class DeviceConfig(Base):
    """Config-Objekt aus api-contract.md Abschnitt 2, pro Geraet editierbar (ab Phase 3)."""

    __tablename__ = "device_config"

    device_id: Mapped[str] = mapped_column(ForeignKey("devices.id"), primary_key=True)
    interval_s: Mapped[int] = mapped_column(Integer, default=DEFAULT_DEVICE_CONFIG["interval_s"])
    moisture_min_pct: Mapped[float] = mapped_column(Float, default=DEFAULT_DEVICE_CONFIG["moisture_min_pct"])
    moisture_target_pct: Mapped[float] = mapped_column(Float, default=DEFAULT_DEVICE_CONFIG["moisture_target_pct"])
    auto_water: Mapped[bool] = mapped_column(Boolean, default=DEFAULT_DEVICE_CONFIG["auto_water"])
    max_pump_s_per_run: Mapped[float] = mapped_column(Float, default=DEFAULT_DEVICE_CONFIG["max_pump_s_per_run"])
    pump_cooldown_s: Mapped[float] = mapped_column(Float, default=DEFAULT_DEVICE_CONFIG["pump_cooldown_s"])
    max_pump_s_per_day: Mapped[float] = mapped_column(Float, default=DEFAULT_DEVICE_CONFIG["max_pump_s_per_day"])
    buzzer_enabled: Mapped[bool] = mapped_column(Boolean, default=DEFAULT_DEVICE_CONFIG["buzzer_enabled"])
    tank_capacity_ml: Mapped[float] = mapped_column(Float, default=DEFAULT_DEVICE_CONFIG["tank_capacity_ml"])
    pump_flow_ml_per_s: Mapped[float] = mapped_column(Float, default=DEFAULT_DEVICE_CONFIG["pump_flow_ml_per_s"])
    tank_low_pct: Mapped[float] = mapped_column(Float, default=DEFAULT_DEVICE_CONFIG["tank_low_pct"])

    device: Mapped["Device"] = relationship(back_populates="config")

    def as_dict(self) -> dict:
        return {
            "interval_s": self.interval_s,
            "moisture_min_pct": self.moisture_min_pct,
            "moisture_target_pct": self.moisture_target_pct,
            "auto_water": self.auto_water,
            "max_pump_s_per_run": self.max_pump_s_per_run,
            "pump_cooldown_s": self.pump_cooldown_s,
            "max_pump_s_per_day": self.max_pump_s_per_day,
            "buzzer_enabled": self.buzzer_enabled,
            "tank_capacity_ml": self.tank_capacity_ml,
            "pump_flow_ml_per_s": self.pump_flow_ml_per_s,
            "tank_low_pct": self.tank_low_pct,
        }


class PendingCommand(Base):
    """Wartende Ein-mal-Befehle fuer ein Geraet (admin-gesteuert, ab Phase 3 per API).

    Phase 1: Tabelle existiert, es gibt aber noch keinen Admin-Endpunkt zum
    Setzen -> pump_run_s/buzzer/identify/tank_refilled sind bis dahin immer
    "leer" (0/false).
    """

    __tablename__ = "pending_commands"

    device_id: Mapped[str] = mapped_column(ForeignKey("devices.id"), primary_key=True)
    pump_run_s: Mapped[float] = mapped_column(Float, default=0)
    buzzer: Mapped[bool] = mapped_column(Boolean, default=False)
    identify: Mapped[bool] = mapped_column(Boolean, default=False)
    tank_refilled: Mapped[bool] = mapped_column(Boolean, default=False)

    def as_dict_and_clear(self) -> dict:
        """Liefert den aktuellen Befehl und setzt ihn danach zurueck (genau einmal ausliefern)."""
        out = {
            "pump_run_s": self.pump_run_s,
            "buzzer": self.buzzer,
            "identify": self.identify,
            "tank_refilled": self.tank_refilled,
        }
        self.pump_run_s = 0
        self.buzzer = False
        self.identify = False
        self.tank_refilled = False
        return out


class Reading(Base):
    """Ein Messwert-POST vom ESP (api-contract.md Abschnitt 2)."""

    __tablename__ = "readings"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    device_id: Mapped[str] = mapped_column(ForeignKey("devices.id"), index=True)
    received_at: Mapped[dt.datetime] = mapped_column(DateTime(timezone=True), default=_utcnow, index=True)

    fw_version: Mapped[str] = mapped_column(String(32))
    seq: Mapped[int] = mapped_column(Integer)
    uptime_s: Mapped[int] = mapped_column(Integer)
    rssi_dbm: Mapped[int] = mapped_column(Integer)

    soil_moisture_pct: Mapped[float | None] = mapped_column(Float, nullable=True)
    soil_moisture_raw: Mapped[int | None] = mapped_column(Integer, nullable=True)
    light_pct: Mapped[float | None] = mapped_column(Float, nullable=True)
    light_raw: Mapped[int | None] = mapped_column(Integer, nullable=True)
    air_temp_c: Mapped[float | None] = mapped_column(Float, nullable=True)
    air_humidity_pct: Mapped[float | None] = mapped_column(Float, nullable=True)

    tank_remaining_ml: Mapped[float] = mapped_column(Float)
    water_level_pct: Mapped[float] = mapped_column(Float)
    pump_on_s_since_last: Mapped[float] = mapped_column(Float)
    pump_running: Mapped[bool] = mapped_column(Boolean)
    auto_water_triggered: Mapped[bool] = mapped_column(Boolean)

    errors: Mapped[list] = mapped_column(JSON, default=list)
    demo_overrides: Mapped[list] = mapped_column(JSON, default=list)

    device: Mapped["Device"] = relationship(back_populates="readings")
