"""Pydantic-Schemas fuer /api/v1/readings, exakt nach .claude/api-contract.md v1.2."""
from __future__ import annotations

from pydantic import BaseModel, ConfigDict, Field, field_validator, model_validator

from .demo import FORCEABLE_ERRORS, MAX_DEMO_S, OVERRIDE_RANGES

DEVICE_ID_PATTERN = r"^[a-z0-9-]{1,32}$"


class ReadingIn(BaseModel):
    model_config = ConfigDict(extra="forbid")

    device_id: str = Field(pattern=DEVICE_ID_PATTERN, max_length=32)
    fw_version: str
    seq: int = Field(ge=0)
    uptime_s: int = Field(ge=0)
    rssi_dbm: int

    soil_moisture_pct: float | None = Field(default=None, ge=0, le=100)
    soil_moisture_raw: int | None = Field(default=None, ge=0, le=4095)
    light_pct: float | None = Field(default=None, ge=0, le=100)
    light_raw: int | None = Field(default=None, ge=0, le=4095)
    air_temp_c: float | None = None
    air_humidity_pct: float | None = None

    tank_remaining_ml: float = Field(ge=0)
    water_level_pct: float = Field(ge=0, le=100)
    pump_on_s_since_last: float = Field(ge=0)
    pump_running: bool
    auto_water_triggered: bool

    errors: list[str] = Field(default_factory=list)
    demo_overrides: list[str] = Field(default_factory=list)


class CommandsOut(BaseModel):
    pump_run_s: float = 0
    buzzer: bool = False
    identify: bool = False
    tank_refilled: bool = False


class ConfigOut(BaseModel):
    interval_s: int
    moisture_min_pct: float
    moisture_target_pct: float
    auto_water: bool
    max_pump_s_per_run: float
    pump_cooldown_s: float
    max_pump_s_per_day: float
    buzzer_enabled: bool
    tank_capacity_ml: float
    pump_flow_ml_per_s: float
    tank_low_pct: float


class ReadingResponse(BaseModel):
    ok: bool = True
    commands: CommandsOut
    config: ConfigOut
    demo: dict | None = None


class ErrorResponse(BaseModel):
    ok: bool = False
    error: str
    detail: str | None = None


class LatestReadingOut(BaseModel):
    """Fuer GET /api/v1/devices/{id}/latest (React-Dashboard, Phase 1)."""

    model_config = ConfigDict(from_attributes=True)

    device_id: str
    received_at: str
    fw_version: str
    seq: int
    uptime_s: int
    rssi_dbm: int
    soil_moisture_pct: float | None
    soil_moisture_raw: int | None
    light_pct: float | None
    light_raw: int | None
    air_temp_c: float | None
    air_humidity_pct: float | None
    tank_remaining_ml: float
    water_level_pct: float
    pump_on_s_since_last: float
    pump_running: bool
    auto_water_triggered: bool
    errors: list[str]
    demo_overrides: list[str]


class DemoIn(BaseModel):
    """PUT /api/v1/devices/{id}/demo"""

    model_config = ConfigDict(extra="forbid")

    overrides: dict[str, float] = Field(default_factory=dict)
    force_errors: list[str] = Field(default_factory=list)
    duration_s: int = Field(default=120, ge=10, le=MAX_DEMO_S)
    scenario: str | None = Field(default=None, max_length=32)

    @field_validator("overrides")
    @classmethod
    def _check_overrides(cls, v: dict[str, float]) -> dict[str, float]:
        for key, value in v.items():
            if key not in OVERRIDE_RANGES:
                raise ValueError(f"Feld '{key}' darf nicht ueberschrieben werden")
            lo, hi = OVERRIDE_RANGES[key]
            if not lo <= value <= hi:
                raise ValueError(f"{key} muss zwischen {lo} und {hi} liegen")
        return v

    @field_validator("force_errors")
    @classmethod
    def _check_errors(cls, v: list[str]) -> list[str]:
        for e in v:
            if e not in FORCEABLE_ERRORS:
                raise ValueError(f"Fehlercode '{e}' nicht erlaubt ({', '.join(FORCEABLE_ERRORS)})")
        return sorted(set(v))

    @model_validator(mode="after")
    def _not_empty(self) -> "DemoIn":
        if not self.overrides and not self.force_errors:
            raise ValueError("mindestens ein Override oder ein Fehler noetig (zum Beenden DELETE benutzen)")
        return self


class DemoOut(BaseModel):
    active: bool
    overrides: dict[str, float] = Field(default_factory=dict)
    force_errors: list[str] = Field(default_factory=list)
    scenario: str | None = None
    started_at: str | None = None
    expires_at: str | None = None
    remaining_s: int = 0


class CommandIn(BaseModel):
    """POST /api/v1/devices/{id}/commands - Ein-mal-Befehle an den ESP"""

    model_config = ConfigDict(extra="forbid")

    pump_run_s: float | None = Field(default=None, gt=0, le=60)
    tank_refilled: bool = False
    identify: bool = False
    buzzer: bool = False

    @model_validator(mode="after")
    def _not_empty(self) -> "CommandIn":
        if not (self.pump_run_s or self.tank_refilled or self.identify or self.buzzer):
            raise ValueError("kein Befehl angegeben")
        return self
