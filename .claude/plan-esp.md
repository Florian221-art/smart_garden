# Plan Claude-ESP (Florian)

Zuständig für: **ESP32-Firmware (C++)**, **Test-Tools** (`tools/`), **Hardware-Doku** (`docs/hardware/`), Beiträge zu Sicherheitsanalyse und Präsentation (ESP-Teil).
Nicht zuständig: KI-Modul, Server, Dashboard (→ Claude-Web).
Schnittstelle zum Server: `.claude/api-contract.md` (v1.2) · Regeln: `.claude/CLAUDE.md`
Pinout + Verkabelung: **`docs/hardware/verkabelung.md`**

## Rahmenbedingungen (mit Florian geklärt)

- Firmware in **C++** (Arduino-Framework), geflasht mit der **Arduino IDE**. Florian lädt nur hoch → muss ohne Anpassungen kompilieren; nur `secrets.h` ausfüllen.
- **Netzwerk:** Pi-Hotspot `SmartGarden`, Server `10.42.0.1`
- **Kein Füllstandssensor:** Tank wird über Pumpenlaufzeit geschätzt (Vertrag Abschnitt 3)
- **LED-Bar = Bodenfeuchte** mit Farbverlauf Grün → Rot
- **Demo-Modus** ist Pflicht: Sensorwerte überschreibbar, um Fehler, Bewässerung und Benachrichtigungen auszulösen (Vertrag Abschnitt 4)
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

## LED-Bar: Bodenfeuchte Grün → Rot

Die Grove LED Bar v2.0 hat **feste Farben**: Segment 1 rot, Segment 2 gelb/orange, Segmente 3–10 grün. Deshalb als Füllstandsanzeige ab dem roten Ende:

| Bodenfeuchte | Leuchtende Segmente | Eindruck |
|---|---|---|
| < 10 % | 1 (nur rot), blinkt | sehr trocken |
| 10–20 % | 1–2 (rot + gelb) | trocken |
| 20–100 % | 3–10 (bis in den grünen Bereich) | ok bis nass |

- Skalierung: `segmente = ceil(feuchte / 10)`, mindestens 1; Helligkeit per Library-Level
- Grün = viel Wasser, Rot = trocken; kurz vorm Gießen ist nur noch Rot/Gelb an
- Sonderzustände: Tank leer → Segment 1 blinkt schnell; `identify` → Lauflicht 5 s; Sensorfehler → abwechselnd Segment 1/10
- Ausrichtung (`greenToRed` in der Library) beim Aufbau testen, damit die Bar vom roten Ende aus füllt

## Demo-Modus auf dem ESP

- Übernimmt `demo.overrides` / `demo.force_errors` aus der Serverantwort; eigener Ablauf-Timer (`expires_in_s`)
- Überschriebene Werte laufen durch die komplette Logik (Pumpe, LED-Bar, Summer, Tank-Schutz); **Pumpen-Sicherheitsgrenzen gelten immer**
- `demo_overrides` im POST listet die nicht echten Felder; Rohwerte bleiben echt
- Serielle Befehle als Fallback ohne Server: `demo soil 12`, `demo light 2`, `demo temp 38`, `demo hum 25`, `demo tank 4`, `demo error dht`, `demo off`, `status`
- Serielles Log markiert Demo-Werte deutlich (`[DEMO]`)

## Firmware (`firmware/smart_garden/`)

Aufbau (Tabs in der Arduino IDE):

| Datei | Inhalt |
|---|---|
| `smart_garden.ino` | `setup()`/`loop()`, nicht-blockierender Scheduler mit `millis()` |
| `config.h` | Pins, Kalibrierwerte, Standard-Config, Firmware-Version |
| `secrets.h.example` | Vorlage → `secrets.h` (WLAN, Server-URL, API-Key, Geräte-ID) |
| `sensors.h/.cpp` | Boden, Licht, DHT11; Median-Filter, Kalibrierung, Fehlercodes |
| `demo.h/.cpp` | Overrides, erzwungene Fehler, Ablauf-Timer, serielle Demo-Befehle |
| `pump.h/.cpp` | Relais, Sicherheitsgrenzen, Cooldown, Tageslimit, Laufzeitzähler |
| `tank.h/.cpp` | Tank-Schätzung, Speicherung in NVS (Preferences), Nachfüllen |
| `display.h/.cpp` | LED-Bar Grün→Rot, Summer-Muster, Status-LED |
| `network.h/.cpp` | WLAN-Reconnect, HTTP(S)-POST, JSON bauen/parsen (ArduinoJson) |
| `remote_config.h/.cpp` | Config aus Serverantwort übernehmen, prüfen, in NVS speichern |
| `README.md` | Arduino-Setup, Bibliotheken, Upload-Schritte, Demo-Befehle |

Datenfluss pro Zyklus: `sensors` (echt) → `demo` (überschreiben) → `tank`/`pump`-Logik → `display` → `network` (POST) → Antwort → `remote_config` + `demo` + Commands.

Arduino-Setup:

- Boardverwalter: **esp32 by Espressif Systems**, Board **ESP32 Dev Module**
- Bibliotheken: **DHT sensor library** (Adafruit) + **Adafruit Unified Sensor**, **Grove LED Bar** (Seeed), **ArduinoJson** (Benoit Blanchon, v7)

Funktionen:

1. WLAN mit Auto-Reconnect, Status auf Onboard-LED
2. Sensoren: Median aus 5 Messungen, roh → %, Fehlercodes; Bodensensor nur beim Messen versorgen
3. POST alle `interval_s` nach Vertrag, Antwort auswerten (`commands`, `config`, `demo`), Config in NVS
4. **Lokale Bewässerung** mit harten Grenzen (Laufzeit/Lauf, Pause, Tageslimit), auch ohne Server; Relais beim Boot sicher AUS
5. **Tank-Schätzung** in NVS, `tank_refilled` per Command oder BOOT-Taste 3 s; Trockenlaufschutz ≤ 5 %
6. **LED-Bar Grün→Rot** für Bodenfeuchte (siehe oben)
7. Summer bei kritischen Zuständen, per Config abschaltbar
8. **Demo-Modus** (siehe oben)
9. Serielles Log 115200 Baud; Test-Sketches je Bauteil in `firmware/tests/`
10. Phase 3: HTTPS mit hinterlegtem Zertifikat des Pi (Certificate Pinning)

## Tools (`tools/`)

- `fake_esp.py`: simuliert den ESP inkl. Tank, Befehlen und Demo-Modus – damit Claude-Web ohne Hardware testen kann

## Reihenfolge

| Phase | Inhalt |
|---|---|
| 0 | Plan, Vertrag, Verkabelung (✓), `tools/fake_esp.py` |
| 1 | Test-Sketches je Bauteil; Firmware liest alle Sensoren + serielle Ausgabe; LED-Bar Grün→Rot |
| 2 | POST an Server, Summer, Kalibrierung, Demo-Modus (seriell + Server) |
| 3 | Relais/Pumpe, lokale Bewässerung, Tank-Schätzung, HTTPS/Pinning |
| 4 | Tests, Fotos/Schaltplan, Beitrag Sicherheitsanalyse & Präsentation |

## Status

Erledigt:

- [x] Aufgabe gelesen, Hardware gesichtet
- [x] Plan, API-Vertrag v1.2, Regeln (`.claude/`)
- [x] Pinout + Verkabelungsplan (`docs/hardware/verkabelung.md`)
- [x] Entscheidungen: KI → Web, Hotspot, Arduino IDE/C++, Tank-Schätzung, Stack Web = Python + React + Tailwind, LED-Bar Grün→Rot, Demo-Modus

Als Nächstes:

- [ ] `tools/fake_esp.py`
- [ ] Test-Sketches je Bauteil
- [ ] Firmware Phase 1–2

Blocker:

- WLAN-Passwort des Pi-Hotspots (kommt von Nico)
