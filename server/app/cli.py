"""CLI zum Anlegen eines Geraets + API-Key (Phase 1: keine Admin-UI noetig).

Nutzung (aus server/, venv aktiv):
    python -m app.cli create-device esp32-kuebel-01
Gibt den KLARTEXT-Key EINMAL aus - danach nur der Hash in der DB.

Fuer das Pi-Installskript (deploy/install_pi.sh):
    python -m app.cli issue-key esp32-kuebel-01   # legt an ODER erneuert Key, gibt nur den Key aus
    python -m app.cli has-device esp32-kuebel-01  # Exit-Code 0 = vorhanden, 1 = fehlt
"""
from __future__ import annotations

import argparse
import secrets
import sys

from .auth import hash_api_key
from .config import DEFAULT_DEVICE_CONFIG
from .db import SessionLocal, init_db
from .models import Device, DeviceConfig


def create_device(device_id: str, label: str | None = None) -> None:
    init_db()
    db = SessionLocal()
    try:
        if db.get(Device, device_id) is not None:
            print(f"Geraet '{device_id}' existiert bereits.", file=sys.stderr)
            sys.exit(1)

        raw_key = secrets.token_urlsafe(32)
        device = Device(id=device_id, api_key_hash=hash_api_key(raw_key), label=label)
        db.add(device)
        db.add(DeviceConfig(device_id=device_id, **DEFAULT_DEVICE_CONFIG))
        db.commit()

        print(f"Geraet '{device_id}' angelegt.")
        print(f"X-API-Key: {raw_key}")
        print("(Diesen Key jetzt sichern - er wird nicht erneut angezeigt.)")
    finally:
        db.close()


def issue_key(device_id: str, label: str | None = None) -> None:
    """Legt das Geraet an, falls noetig, sonst wird nur der Key-Hash ersetzt.

    Messwerte/Config bleiben erhalten. Gibt NUR den Klartext-Key auf stdout aus,
    damit Skripte ihn direkt einlesen koennen.
    """
    init_db()
    db = SessionLocal()
    try:
        raw_key = secrets.token_urlsafe(32)
        device = db.get(Device, device_id)
        if device is None:
            db.add(Device(id=device_id, api_key_hash=hash_api_key(raw_key), label=label))
            db.add(DeviceConfig(device_id=device_id, **DEFAULT_DEVICE_CONFIG))
        else:
            device.api_key_hash = hash_api_key(raw_key)
        db.commit()
        print(raw_key)
    finally:
        db.close()


def has_device(device_id: str) -> None:
    init_db()
    db = SessionLocal()
    try:
        sys.exit(0 if db.get(Device, device_id) is not None else 1)
    finally:
        db.close()


def main() -> None:
    parser = argparse.ArgumentParser(description="Smart Garden Geraete-Verwaltung")
    sub = parser.add_subparsers(dest="command", required=True)

    create = sub.add_parser("create-device", help="Neues Geraet + API-Key anlegen")
    create.add_argument("device_id", help="z.B. esp32-kuebel-01")
    create.add_argument("--label", default=None, help="Menschenlesbarer Name")

    issue = sub.add_parser("issue-key", help="Geraet anlegen oder API-Key erneuern (nur Key auf stdout)")
    issue.add_argument("device_id")
    issue.add_argument("--label", default=None)

    has = sub.add_parser("has-device", help="Exit-Code 0, wenn das Geraet existiert")
    has.add_argument("device_id")

    args = parser.parse_args()
    if args.command == "create-device":
        create_device(args.device_id, args.label)
    elif args.command == "issue-key":
        issue_key(args.device_id, args.label)
    elif args.command == "has-device":
        has_device(args.device_id)


if __name__ == "__main__":
    main()
