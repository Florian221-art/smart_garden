# Plan Claude-ESP (Florian)

Zuständig für: **ESP32-Firmware (C++)**, **Test-Tools** (`tools/`), **Hardware-Doku** (`docs/hardware/`), Beiträge zu Sicherheitsanalyse und Präsentation (ESP-Teil).
Nicht zuständig: KI-Modul, Server, Dashboard (→ Claude-Web).
Schnittstelle zum Server: `.claude/api-contract.md` (v1.2) · Regeln: `.claude/CLAUDE.md`
Pinout + Verkabelung: **`docs/hardware/verkabelung.md`** · Firmware-Anleitung: **`firmware/smart_garden/README.md`**

## Rahmenbedingungen (mit Florian geklärt)

- Firmware in **C++** (Arduino-Framework, ESP32-Core 3.x), geflasht mit der **Arduino IDE**. Florian lädt nur hoch → muss ohne Anpassungen kompilieren; nur `secrets.h` ausfüllen.
- **Netzwerk:** Pi-Hotspot `SmartGarden`, Server `10.42.0.1`
- **Kein Füllstandssensor:** Tank wird über Pumpenlaufzeit geschätzt (Vertrag Abschnitt 3)
- **LED-Bar = Bodenfeuchte** mit Farbverlauf Rot (trocken) → Grün (feucht)
- **Demo-Modus** ist Pflicht (Vertrag Abschnitt 4)
- **Bodensensor gewechselt** (28.09.): kapazitiver „Capacitive Soil Moisture Sensor v2.0“ (HW-390) statt resistivem IDUINO → dauerhaft an 3V3, GPIO25 frei
- Vorhanden: Grove-Kabel, 12-V-Netzteil, Schlauch, Wasserbehälter, Grove Relay, Micro-USB-Kabel. Freilaufdiode optional. Keine Widerstände nötig.
- HTTP während der Entwicklung, HTTPS mit Certificate Pinning in Phase 3 (in Firmware schon vorbereitet: `SERVER_CA_CERT`)

## Hardware

| Teil | Typ | ESP32-Pin |
|---|---|---|
| Mikrocontroller | ESP32 DevKit (ESP-WROOM-32, CP2102, 30 Pin) | – |
| Bodenfeuchte | Capacitive Soil Moisture Sensor v2.0 (HW-390) | AOUT → GPIO34, VCC → 3V3 |
| Licht | MH-Sensor LDR-Modul | AO → GPIO35 |
| Temp./Luftfeuchte | Grove v1.2 (**DHT11**) | SIG → GPIO4 |
| Anzeige | Grove LED Bar v2.0 (MY9221) | DI → GPIO18, DCKI → GPIO19 |
| Summer | Piezo LF-PB30W35B (aktiv) | GPIO26 (direkt) |
| Pumpen-Relais | Grove Relay HLS8L-DC3V-S-C | SIG → GPIO27 |
| Pumpe | 12 V DC 385 | über Relais |
| Taster „Tank voll“ | BOOT-Taste onboard | GPIO0 |
| Status-LED | Onboard | GPIO2 |

## Firmware (`firmware/smart_garden/`) – v0.1.0 fertig, kompiliert (ESP32-Core 3.3.12, 81 % Flash)

| Datei | Inhalt |
|---|---|
| `smart_garden.ino` | `setup()`/`loop()`, Messzyklus, Auto-Bewässerung, JSON senden, Serverantwort, serielle Konsole |
| `garden_config.h/.cpp` | Pins, harte Sicherheitsgrenzen, Settings (Server-Config) + Kalibrierung in NVS |
| `secrets.h.example` | Vorlage → `secrets.h` (WLAN, Server-URL, API-Key, Geräte-ID, optional CA-Zertifikat) |
| `sensors.h/.cpp` | Bodenfeuchte kapazitiv, LDR, DHT11; Median-Filter, Kalibrierung |
| `demo.h/.cpp` | Overrides, erzwungene Fehler, Ablauf-Timer, Anbindung an Server + Konsole |
| `pump.h/.cpp` | Relais nicht blockierend, max. Laufzeit, Cooldown, Tageslimit, Laufzeitzähler |
| `tank.h/.cpp` | Tank-Schätzung in NVS, Nachfüllen |
| `display.h/.cpp` | LED-Bar Rot→Grün, Summer-Muster (nicht blockierend), Status-LED |
| `garden_net.h/.cpp` | WLAN-Reconnect, HTTP(S)-POST, JSON-Antwort parsen |
| `README.md` | Arduino-Setup, Bibliotheken, Upload, Kalibrierung, Konsolenbefehle |

Hinweise für spätere Änderungen:

- Dateinamen `garden_net.*` / `garden_config.*` bewusst so gewählt – `Network.h`/`config.h` kollidieren mit dem ESP32-Core 3.x (Windows ist nicht case-sensitiv).
- Vorwärtsdeklarationen oben im `.ino` beibehalten (stabil gegenüber Arduino-Prototyp-Generierung).
- Kompiliertest in der Cloud-Umgebung: arduino-cli mit manuell installiertem Core (GitHub-Releases), Bibliotheken per `git clone`.

## Tools (`tools/`)

- `fake_esp.py` (PR #3): simuliert den ESP inkl. Tank, Befehlen und Demo-Modus

## Reihenfolge

| Phase | Inhalt | Stand |
|---|---|---|
| 0 | Plan, Vertrag, Verkabelung, `tools/fake_esp.py` | ✓ |
| 1–3 | Komplette Firmware (Sensoren, LED-Bar, Summer, Demo, Pumpe, Tank, POST) | ✓ kompiliert, **Hardwaretest ausstehend** |
| 3b | Kalibrierung mit echter Hardware, Durchfluss messen, Test gegen echten Server | offen |
| 3c | HTTPS mit Caddy-Root-Zertifikat (`SERVER_CA_CERT`) | offen |
| 4 | Fotos/Schaltplan, Beitrag Sicherheitsanalyse (ESP-Teil) & Präsentation | offen |

## Status

Erledigt:

- [x] Plan, API-Vertrag v1.2, Regeln (`.claude/`)
- [x] Pinout + Verkabelungsplan v2 (kapazitiver Sensor)
- [x] `tools/fake_esp.py` (PR #3)
- [x] Firmware v0.1.0 komplett, kompiliert ohne Warnungen

Als Nächstes:

- [ ] Florian: aufbauen, hochladen, Inbetriebnahme nach `verkabelung.md` Abschnitt 6, Kalibrierung
- [ ] Feedback aus Hardwaretest einarbeiten (LED-Bar-Richtung, Summer-Lautstärke, Rohwerte)
- [ ] Test gegen Nicos Server, sobald `POST /api/v1/readings` läuft
- [ ] ESP-Teil für `docs/server/security.md` (Bedrohungen: API-Key im Flash, unverschlüsseltes WLAN-Passwort, Manipulation der Pumpe → harte Grenzen)

Blocker:

- WLAN-Passwort des Pi-Hotspots + API-Key (kommt von Nico)
