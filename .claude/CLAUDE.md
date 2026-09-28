# Smart Garden – Regeln für alle Claude-Instanzen

Hackathon-Projekt (Euregio, 48 h): intelligenter Pflanzkübel für einen Schulcampus.
Aufgabe: `docs/Intelligenter Gemüsegarten Pflanzkübel DE.pdf`

Zwei Claude-Instanzen arbeiten parallel in diesem Repo:

| Instanz | Mensch | Bereich | Plan + Status |
|---|---|---|---|
| **Claude-ESP** | Florian Schoenen | ESP32-Firmware (Arduino IDE), Test-Tools, Hardware-Doku | `.claude/plan-esp.md` |
| **Claude-Web** | Nico Steins | Webserver/API, Datenbank, Dashboard, **KI-Modul**, Pi-Deployment inkl. WLAN-Hotspot | `.claude/plan-webserver.md` |

Gemeinsame Schnittstelle: **`.claude/api-contract.md`** – vor jeder Arbeit lesen.
Verkabelung/Pinout: `docs/hardware/verkabelung.md`

## Wo was hingehört

- **Anweisungen für Claude, Pläne, Status, Aufgabenlisten: ausschließlich im Ordner `.claude/`.**
- Code und Projekt-Doku für Menschen (Verkabelung, Sicherheitsanalyse, Diagramme) in die jeweiligen Projektordner.

## Git-Regeln (verbindlich)

1. **Nie direkt auf `main` pushen.** Nur in eigene Branches, dann Pull Request. Gemerged wird nur von Florian oder Nico.
2. Branch-Präfixe: Claude-ESP → `feature/esp-*`, `docs/*` · Claude-Web → `feature/server-*`, `feature/web-*`, `feature/ai-*`, `deploy/*`
3. **Ordner-Besitz** – jede Instanz schreibt nur in ihre eigenen Pfade:
   - Claude-ESP: `firmware/`, `tools/`, `docs/hardware/`, `.gitignore` (Root), `.claude/CLAUDE.md`, `.claude/plan-esp.md`, `.claude/api-contract.md`
   - Claude-Web: `server/`, `web/`, `ai/`, `deploy/`, `docs/server/`, `.claude/plan-webserver.md`
   - Eigene Ignore-Regeln als `.gitignore` im eigenen Ordner (z. B. `server/.gitignore`).
4. Vor jeder Arbeit: `git fetch` und eigenen Branch auf aktuellen `main` bringen.
5. **API-Vertrag ändern** nur per PR, der ausschließlich `.claude/api-contract.md` ändert, mit `[CONTRACT]` im PR-Titel. Beide Menschen müssen zustimmen.
6. Keine Secrets committen (WLAN-Passwort, API-Keys, Passwörter). Beispieldateien heißen `*.example`.

## Kommunikation zwischen den Instanzen

- Fragen/Wünsche an die andere Seite als **GitHub Issue** mit Label `an-esp` bzw. `an-web`.
- Jede Instanz pflegt den Abschnitt **Status** in ihrer Plan-Datei (fertig / als Nächstes / Blocker) und aktualisiert ihn bei jedem PR.

## Sprache

Doku und Commits auf Deutsch; Code-Bezeichner auf Englisch. UI-Texte immer über Übersetzungsschlüssel (NL/DE/EN).
