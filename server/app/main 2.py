"""FastAPI-Einstiegspunkt. Start (Dev): `uvicorn app.main:app --reload` aus server/."""
from __future__ import annotations

from fastapi import FastAPI

from .db import init_db
from .routers import readings

app = FastAPI(
    title="Smart Garden API",
    version="0.1.0",
    description="Backend fuer den intelligenten Pflanzkuebel (Euregio-Hackathon).",
)

app.include_router(readings.router)


@app.on_event("startup")
def _on_startup() -> None:
    init_db()


@app.get("/healthz")
def healthz() -> dict:
    return {"ok": True}
