"""API-Key-Hashing/-Pruefung fuer Geraete (X-API-Key Header).

CLAUDE.md Sicherheitsregel: Keys gehasht speichern, Vergleich zeitkonstant.
Wir nutzen argon2 (bereits fuer User-Passwoerter in Phase 3 vorgesehen) statt
eines rohen HMAC-Vergleichs, da argon2 intrinsisch zeitkonstant vergleicht.
"""
from __future__ import annotations

from argon2 import PasswordHasher
from argon2.exceptions import VerifyMismatchError, InvalidHash

_hasher = PasswordHasher()


def hash_api_key(raw_key: str) -> str:
    return _hasher.hash(raw_key)


def verify_api_key(raw_key: str, hashed: str) -> bool:
    try:
        return _hasher.verify(hashed, raw_key)
    except (VerifyMismatchError, InvalidHash):
        return False
