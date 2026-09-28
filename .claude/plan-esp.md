# Plan Claude-ESP (Florian)

Zuständig für: **ESP32-Firmware (C++)**, **Test-Tools** (`tools/`), **Hardware-Doku** (`docs/hardware/`), Beiträge zu Sicherheitsanalyse und Präsentation (ESP-Teil).
Nicht zuständig: KI-Modul, Server, Dashboard (→ Claude-Web).
Schnittstelle zum Server: `.claude/api-contract.md` · Regeln: `.claude/CLAUDE.md`
Pinout + Verkabelung: **`docs/hardware/verkabelung.md`**

## Rahmenbedingungen (mit Florian geklärt)

- Firmware in **C++** (Arduino-Framework), geflasht mit der **Arduino IDE**. Florian lädt nur hoch → muss ohne Anpassungen kompilieren; nur `secrets.h` ausfüllen.
- **Netzwerk:** Pi-Hotspot `SmartGarden`, Server `10.42.0.1`
- **Kein Füllstandssensor:** Tank wird über Pumpenlaufzeit geschätzt (Vertrag Abschnitt 3)
- Vorhanden: Grove-Kabel, 12-V-Netzteil, Schlauch, Wasserbehälter, Grove Relay, Micro-USB-Kabel. Freilaufdiode optional.
- HTTP während der Entwicklung, HTTPS mit Certificate Pinning in Phase 3

## Hardware

| Teil | Typ | ESP32-Pin |
|---|---|---|
| Mikrocontroller | ESP32 DevKit (ESP-WROOM-32, CP2102, 30 Pin) | – |
| Bodenfeuchte | IDUINO resistiv (S/+/−) | S → GPIO34, + → GPIO25 (geschaltet) |
| Licht | MH-Sensor LDR-Modul | AO → GPIO35 |
| Temp./Luftfeuchte | Grove v1.2 (**DHT11**) | SIG → GPIO4 |
| Anzeige | Grove LED Bar v2.0 (MY9221) | DI → GPIO18, DCKI → GPIO19 |
| Summer | Piezo LF-PB30W35B (aktiv) | GPIO26 |
| Pumpen-Relais | Grove Relay HLS8L-DC3V-S-C | SIG → GPIO27 |
| Pumpe | 12 V DC 385 | über Relais |
| Taster „Tank voll“ | BOOT-Taste onboard | GPIO0 (nur nach dem Booten lesen) |
| Status-LED | Onboard | GPIO2 |

## Firmware (`firmware/smart_garden/`)

Aufbau (Tabs in der Arduino IDE):

| Datei | Inhalt |
|---|---|
| `smart_garden.ino` | `setup()`/`loop()`, nicht-blockierender Scheduler mit `millis()` |
| `config.h` | Pins, Kalibrierwerte, Standard-Config, Firmware-Version |
| `secrets.h.example` | Vorlage → `secrets.h` (WLAN, Server-URL, API-Key, Geräte-ID) |
| `sensors.h/.cpp` | Boden, Licht, DHT11; Median-Filter, Kalibrierung, Fehlercodes |
| `pump.h/.cpp` | Relais, Sicherheitsgrenzen, Cooldown, Tageslimit, Laufzeitzähler |
| `tank.h/.cpp` | Tank-Schätzung, Speicherung in NVS (Preferences), Nachfüllen |
| `display.h/.cpp` | LED-Bar-Anzeige, Summer-Muster, Status-LED |
| `network.h/.cpp` | WLAN-Reconnect, HTTP(S)-POST, JSON bauen/parsen (ArduinoJson) |
| `remote_config.h/.cpp` | Config aus Serverantwort übernehmen, prüfen, in NVS speichern |
| `README.md` | Arduino-Setup, Bibliotheken, Upload-Schritte |

Arduino-Setup:

- Boardverwalter: **esp32 by Espressif Systems**, Board **ESP32 Dev Module**
- Bibliotheken: **DHT sensor library** (Adafruit) + **Adafruit Unified Sensor**, **Grove LED Bar** (Seeed), **ArduinoJson** (Benoit Blanchon, v7)

Funktionen:

1. WLAN mit Auto-Reconnect, Status auf Onboard-LED
2. Sensoren: Median aus 5 Messungen, roh → %, Fehlercodes; Bodensensor nur beim Messen versorgen
3. POST alle `interval_s` nach Vertrag, Antwort auswerten (`commands`, `config`), Config in NVS
4. **Lokale Bewässerung** mit harten Grenzen (Laufzeit/Lauf, Pause, Tageslimit), auch ohne Server; Relais beim Boot sicher AUS
5. **Tank-Schätzung** in NVS, `tank_refilled` per Command oder BOOT-Taste 3 s; Trockenlaufschutz ≤ 5 %
6. LED-Bar: Bodenfeuchte 0–10 Segmente; blinkt bei Tank leer / identify
7. Summer bei kritischen Zuständen, per Config abschaltbar
8. Serielles Log 115200 Baud; Test-Sketches je Bauteil in `firmware/tests/`
9. Phase 3: HTTPS mit hinterlegtem Zertifikat des Pi (Certificate Pinning)

## Tools (`tools/`)

- `fake_esp.py`: simuliert den ESP inkl. Tank und Befehlen – damit Claude-Web ohne Hardware testen kann

## Reihenfolge

| Phase | Inhalt |
|---|---|
| 0 | Plan, Vertrag, Verkabelung (✓), `tools/fake_esp.py` |
| 1 | Test-Sketches je Bauteil; Firmware liest alle Sensoren + serielle Ausgabe |
| 2 | POST an Server, LED-Bar, Summer, Kalibrierung |
| 3 | Relais/Pumpe, lokale Bewässerung, Tank-Schätzung, HTTPS/Pinning |
| 4 | Tests, Fotos/Schaltplan, Beitrag Sicherheitsanalyse & Präsentation |

## Status

Erledigt:

- [x] Aufgabe gelesen, Hardware gesichtet
- [x] Plan, API-Vertrag v1.1, Regeln (`.claude/`)
- [x] Pinout + Verkabelungsplan (`docs/hardware/verkabelung.md`)
- [x] Entscheidungen: KI → Web, Hotspot, Arduino IDE/C++, Tank-Schätzung, Stack Web = Python + React + Tailwind

Als Nächstes:

- [ ] `tools/fake_esp.py`
- [ ] Test-Sketches je Bauteil
- [ ] Firmware Phase 1–2

Blocker:

- WLAN-Passwort des Pi-Hotspots (kommt von Nico)
