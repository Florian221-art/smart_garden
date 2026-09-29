"""SQLAlchemy Engine/Session-Setup (SQLite, WAL-Modus laut Tech-Stack)."""
from __future__ import annotations

from collections.abc import Generator

from sqlalchemy import create_engine, event
from sqlalchemy.orm import DeclarativeBase, Session, sessionmaker

from .config import DATABASE_URL


class Base(DeclarativeBase):
    pass


engine = create_engine(
    DATABASE_URL,
    connect_args={"check_same_thread": False},
)


@event.listens_for(engine, "connect")
def _set_sqlite_pragmas(dbapi_connection, connection_record) -> None:  # noqa: ANN001
    cursor = dbapi_connection.cursor()
    cursor.execute("PRAGMA journal_mode=WAL")
    cursor.execute("PRAGMA foreign_keys=ON")
    cursor.close()


SessionLocal = sessionmaker(bind=engine, autoflush=False, autocommit=False)


def get_db() -> Generator[Session, None, None]:
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()


def init_db() -> None:
    """Legt alle Tabellen an, falls noch nicht vorhanden (Phase 1: kein Migrationstool)."""
    from . import models  # noqa: F401  (Modelle registrieren)

    Base.metadata.create_all(bind=engine)
    _migrate()


# Einfache Datenmigrationen ueber SQLites "user_version" (jede laeuft genau einmal).
# Kein Migrationstool noetig, solange sich nur Werte und keine Spalten aendern.
SCHEMA_VERSION = 1


def _migrate() -> None:
    with engine.begin() as conn:
        version = conn.exec_driver_sql("PRAGMA user_version").scalar() or 0
        if version < 1:
            # Issue #32: Pumpenstoss 0,5 s statt 5 s. Nur den alten Standardwert ersetzen -
            # bewusst anders eingestellte Werte bleiben unveraendert.
            conn.exec_driver_sql("UPDATE device_config SET max_pump_s_per_run = 0.5 WHERE max_pump_s_per_run = 5")
        if version < SCHEMA_VERSION:
            conn.exec_driver_sql(f"PRAGMA user_version = {SCHEMA_VERSION}")
