"""FastAPI-Einstiegspunkt. Start (Dev): `uvicorn app.main:app --reload` aus server/."""
from __future__ import annotations

import logging

from fastapi import FastAPI

from .db import init_db
from .routers import control, history, readings

# Demo-/Befehls-Aenderungen im Terminal sichtbar machen (vorlaeufiges Zugriffsprotokoll)
logging.basicConfig(level=logging.INFO, format="%(levelname)s:     %(name)s %(message)s")

app = FastAPI(
    title="Smart Garden API",
    version="0.1.0",
    description="Backend fuer den intelligenten Pflanzkuebel (Euregio-Hackathon).",
)

app.include_router(readings.router)
app.include_router(history.router)
app.include_router(control.router)


@app.on_event("startup")
def _on_startup() -> None:
    init_db()


@app.get("/healthz")
def healthz() -> dict:
    return {"ok": True}
