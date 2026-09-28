# Plan Claude-ESP (Florian)

Zuständig für: **ESP32-Firmware**, **Test-Tools** (`tools/`), **Hardware-Doku** (`docs/hardware/`), Beiträge zu Sicherheitsanalyse und Präsentation (ESP-Teil).
Nicht zuständig: KI-Modul (→ Claude-Web).
Schnittstelle zum Server: `.claude/api-contract.md` · Regeln: `.claude/CLAUDE.md`
Pinout + Verkabelung: **`docs/hardware/verkabelung.md`**

## Rahmenbedingungen (mit Florian geklärt)

- **Arduino IDE** zum Flashen. Florian lädt nur hoch → Firmware muss ohne Anpassungen kompilieren; nur `secrets.h` ausfüllen.
- **Netzwerk:** Pi-Hotspot `SmartGarden`, Server `10.42.0.1`
- **Kein Füllstandssensor:** Tank wird über Pumpenlaufzeit geschätzt (Vertrag Abschnitt 3)
- Vorhanden: Grove-Kabel, 12-V-Netzteil, Schlauch, Wasserbehälter, Grove Relay

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

Dateien:

- `smart_garden.ino` – Hauptprogramm
- `config.h` – Pins, Kalibrierwerte, Standard-Config
- `secrets.h.example` → Florian kopiert nach `secrets.h` (WLAN, Server-URL, API-Key; gitignored)
- `README.md` – Board-Paket, Bibliotheken, Upload-Schritte für die Arduino IDE

Arduino-Setup:

- Boardverwalter: **esp32 by Espressif Systems**, Board **ESP32 Dev Module**
- Bibliotheksverwalter: **DHT sensor library** (Adafruit) + **Adafruit Unified Sensor**, **Grove LED Bar** (Seeed), **ArduinoJson** (Benoit Blanchon, v7)

Funktionen:

1. WLAN mit Auto-Reconnect, Status auf Onboard-LED
2. Sensoren: Median aus 5 Messungen, roh → %, Fehlercodes; Bodensensor nur beim Messen versorgen
3. POST alle `interval_s` nach Vertrag, Antwort auswerten (`commands`, `config`), Config im Flash speichern
4. **Lokale Bewässerung** mit harten Grenzen (Laufzeit/Lauf, Pause, Tageslimit), läuft auch ohne Server; Relais beim Boot sicher AUS
5. **Tank-Schätzung** in NVS (Preferences), `tank_refilled` per Command oder BOOT-Taste 3 s; Trockenlaufschutz ≤ 5 %
6. LED-Bar: Bodenfeuchte 0–10 Segmente; blinkt bei Tank leer / identify
7. Summer bei kritischen Zuständen (Tank leer, Sensorfehler), per Config abschaltbar
8. Serielles Log 115200 Baud; Test-Sketch pro Sensor für den Aufbau (`firmware/tests/`)
9. Phase 3: HTTPS mit hinterlegtem Zertifikat des Pi (Certificate Pinning)

## Tools (`tools/`)

- `fake_esp.py`: simuliert den ESP inkl. Tank und Befehlen – damit Claude-Web ohne Hardware testen kann

## Reihenfolge

| Phase | Inhalt |
|---|---|
| 0 | Plan, Vertrag, Verkabelung (✓), `tools/fake_esp.py` |
| 1 | Test-Sketches je Sensor; Firmware liest alle Sensoren + serielle Ausgabe |
| 2 | POST an Server, LED-Bar, Summer, Kalibrierung |
| 3 | Relais/Pumpe, lokale Bewässerung, Tank-Schätzung, HTTPS/Pinning |
| 4 | Tests, Fotos/Schaltplan, Beitrag Sicherheitsanalyse & Präsentation |

## Status

Erledigt:

- [x] Aufgabe gelesen, Hardware gesichtet
- [x] Plan, API-Vertrag v1.1, Regeln (`.claude/`)
- [x] Pinout + Verkabelungsplan (`docs/hardware/verkabelung.md`)
- [x] Offene Fragen geklärt (KI → Web, Hotspot, Arduino IDE, Tank-Schätzung)

Als Nächstes:

- [ ] `tools/fake_esp.py`
- [ ] Test-Sketches je Sensor
- [ ] Firmware Phase 1–2

Blocker:

- WLAN-Passwort des Pi-Hotspots (kommt von Nico)
