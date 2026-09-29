# Smart Garden – intelligenter Pflanzkübel

Hackathon-Projekt (Euregio, 48 h, September 2026) von **Florian Schoenen** und **Nico Steins**.
Die Aufgabe steht in [`docs/Intelligenter Gemüsegarten Pflanzkübel DE.pdf`](docs/Intelligenter%20Gemüsegarten%20Pflanzkübel%20DE.pdf).

Der Pflanzkübel misst Bodenfeuchte, Licht, Temperatur und Luftfeuchte. Er gießt selbstständig und sparsam, zeigt den Zustand direkt am Kübel an und schickt alle Werte an einen Raspberry Pi. Dort speichert ein Server die Daten, ein Dashboard zeigt sie für alle verständlich an (NL/DE/EN), und ein KI-Modul erkennt Auffälligkeiten und schätzt den Wasserbedarf.

## Aufbau

```
 Kübel                                   Raspberry Pi 5 (eigenes WLAN "SmartGarden", 10.42.0.1)
 ┌─────────────────────────────┐         ┌───────────────────────────────────────────────┐
 │ ESP32                       │  WLAN   │ FastAPI-Server ── SQLite                        │
 │  ├ Bodenfeuchte (kapazitiv) │ ──────► │   POST /api/v1/readings  (X-API-Key)            │
 │  ├ Licht (LDR)              │ ◄────── │   Antwort: config · commands · demo             │
 │  ├ Temp./Luftfeuchte (DHT11)│         │ KI-Modul (Python)                               │
 │  ├ LED-Bar (Feuchte-Anzeige)│         │ Dashboard (React + Tailwind) ◄── Handy/Laptop   │
 │  ├ Summer (Alarm)           │         └───────────────────────────────────────────────┘
 │  └ Relais → 12-V-Pumpe      │
 └─────────────────────────────┘
```

Die Schnittstelle zwischen ESP und Server ist in [`.claude/api-contract.md`](.claude/api-contract.md) festgelegt (Version 1.2).

## Ordner

| Ordner | Inhalt | Zuständig | Doku |
|---|---|---|---|
| `firmware/smart_garden/` | ESP32-Firmware (C++, Arduino IDE) | Florian | [README](firmware/smart_garden/README.md) |
| `tools/` | `fake_esp.py`: ESP-Simulator zum Testen ohne Hardware | Florian | [README](tools/README.md) |
| `docs/hardware/` | Pinout, Verkabelung, Sicherheitsanalyse ESP | Florian | [Verkabelung](docs/hardware/verkabelung.md) · [Sicherheit](docs/hardware/sicherheit-esp.md) |
| `server/` | FastAPI-Backend, Datenbank | Nico | `docs/server/` |
| `web/` | Dashboard (React, Vite, Tailwind) | Nico | `web/README.md` |
| `ai/` | KI-Modul (Python) – in Arbeit | Nico | `.claude/plan-webserver.md` |
| `deploy/` | Pi-Einrichtung: Hotspot, systemd-Dienst | Nico | [README](deploy/README.md) |
| `.claude/` | Pläne, Regeln und API-Vertrag für die beiden Claude-Instanzen | beide | [Regeln](.claude/CLAUDE.md) |

## Schnellstart

1. **Pi einrichten**: Hotspot und Server nach [`deploy/README.md`](deploy/README.md). Dabei für den Kübel ein Gerät mit API-Key anlegen.
2. **Kübel verkabeln** nach [`docs/hardware/verkabelung.md`](docs/hardware/verkabelung.md).
3. **Firmware hochladen** nach [`firmware/smart_garden/README.md`](firmware/smart_garden/README.md). WLAN-Passwort und API-Key gehören in `secrets.h`, nicht ins Repo.
4. **Kalibrieren**: `cal soil dry` und `cal soil wet` im seriellen Monitor, dann den Pumpen-Durchfluss messen.
5. **Dashboard** im Hotspot-WLAN öffnen.
6. **Ohne Hardware testen**: `python tools/fake_esp.py --url http://10.42.0.1:8000 --key <api-key>`

## Was die Lösung aus der Aufgabe abdeckt

| Anforderung | Umsetzung |
|---|---|
| Sensordaten (Feuchte, Licht, Temperatur, Wasserstand) | 4 Sensoren am ESP32. Den Wasserstand schätzt die Firmware aus der Pumpenlaufzeit (kein Sensor vorhanden). |
| Wasserverschwendung vermeiden | Gießen in Stößen mit Pause bis zu einer Zielfeuchte, Tageslimit. Die Plausibilitätsprüfung stoppt, wenn Gießen nicht wirkt. |
| Warnungen | Summer und LED-Bar am Kübel, Statusmeldungen im Dashboard |
| Zugänglich | LED-Bar ohne Lesen verständlich (je mehr LEDs, desto trockener), Dashboard mehrsprachig und barrierearm |
| KI | geplant: Anomalie-Erkennung und Vorhersage des Wasserbedarfs (`ai/`, siehe `.claude/plan-webserver.md`) |
| Sicherheit | API-Key pro Gerät, WPA2-Hotspot, harte Pumpengrenzen im ESP, Watchdog, keine Secrets im Repo, Bedrohungsmodell ([ESP-Teil](docs/hardware/sicherheit-esp.md)) |
| Datenschutz | Keine Kamera, kein Mikrofon, keine personenbezogenen Daten am Kübel |
| Demo | Demo-Modus: Sensorwerte überschreiben und Fehler auslösen, per Dashboard oder serieller Konsole |

## Regeln für Beiträge

- **Nie direkt auf `main` pushen.** Immer über einen Branch und einen Pull Request.
- **Keine Secrets committen.** WLAN-Passwort und API-Keys gehören in `secrets.h` bzw. `.env`, beide stehen in `.gitignore`.
- Die API-Schnittstelle nur per `[CONTRACT]`-PR ändern, beide müssen zustimmen.
- Weitere Regeln: [`.claude/CLAUDE.md`](.claude/CLAUDE.md)

Lizenz: siehe [LICENSE](LICENSE).
