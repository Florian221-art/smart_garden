# Smart Garden – Regeln für alle Claude-Instanzen

Hackathon-Projekt (Euregio, 48 h): intelligenter Pflanzkübel für einen Schulcampus.
Aufgabe: `docs/Intelligenter Gemüsegarten Pflanzkübel DE.pdf`

Zwei Claude-Instanzen arbeiten parallel in diesem Repo:

| Instanz | Mensch | Bereich | Plan |
|---|---|---|---|
| **Claude-ESP** | Florian Schoenen | ESP32-Firmware, KI-Modul, Test-Tools | `.claude/plan-esp.md` |
| **Claude-Web** | Nico Steins | Webserver/API, Datenbank, Dashboard, Pi-Deployment | `.claude/plan-webserver.md` |

Gemeinsame Schnittstelle: **`.claude/api-contract.md`** – vor jeder Arbeit lesen.

## Git-Regeln (verbindlich)

1. **Nie direkt auf `main` pushen.** Nur in eigene Branches, dann Pull Request. Gemerged wird von Florian oder Nico.
2. Branch-Präfixe: Claude-ESP → `feature/esp-*`, `feature/ai-*`, `docs/*` · Claude-Web → `feature/server-*`, `feature/web-*`, `deploy/*`
3. **Ordner-Besitz** – jede Instanz schreibt nur in ihre eigenen Ordner:
   - Claude-ESP: `firmware/`, `ai/`, `tools/`, `docs/hardware/`, `CLAUDE.md`, `.claude/plan-esp.md`, `.claude/api-contract.md`
   - Claude-Web: `server/`, `web/`, `deploy/`, `docs/server/`, `.claude/plan-webserver.md`
   - Eigene `.gitignore` bitte im eigenen Ordner anlegen (z. B. `server/.gitignore`), die Root-`.gitignore` gehört Claude-ESP.
4. Vor jeder Arbeit: `git fetch` und eigenen Branch auf aktuellen `main` rebasen/mergen.
5. **API-Vertrag ändern** nur per PR, der ausschließlich `.claude/api-contract.md` ändert, mit Hinweis im PR-Titel `[CONTRACT]`. Beide Menschen müssen zustimmen.
6. Keine Secrets committen (WLAN-Passwort, API-Keys, Passwörter). Beispieldateien heißen `*.example`.

## Kommunikation zwischen den Instanzen

- Fragen/Wünsche an die andere Seite als **GitHub Issue** mit Label `an-esp` bzw. `an-web`.
- Jede Instanz pflegt den Abschnitt **Status** in ihrer Plan-Datei (was fertig ist, was als Nächstes kommt, Blocker).

## Sprache

Doku und Commits auf Deutsch; Code-Bezeichner auf Englisch. UI-Texte immer über Übersetzungsschlüssel (NL/DE/EN).
