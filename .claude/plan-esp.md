# Plan Claude-ESP (Florian)

Zuständig für: **ESP32-Firmware**, **KI-Modul** (`ai/`), **Test-Tools** (`tools/`), Hardware-Doku (`docs/hardware/`).
Schnittstelle zum Server: `.claude/api-contract.md` · Regeln: `.claude/CLAUDE.md`
Vollständiges Pinout + Verkabelung: **`docs/hardware/verkabelung.md`**

## Hardware (Stand 28.09.)

| Teil | Typ | ESP32-Pin |
|---|---|---|
| Mikrocontroller | ESP32 DevKit (ESP-WROOM-32, CP2102, 30 Pin) | – |
| Bodenfeuchte | IDUINO resistiv (S/+/−) | S → GPIO34, + → GPIO25 (geschaltet) |
| Licht | MH-Sensor LDR-Modul (VCC/GND/DO/AO) | AO → GPIO35 |
| Temp./Luftfeuchte | Grove Temperature&Humidity v1.2 (**DHT11**) | SIG → GPIO4 |
| Anzeige | Grove LED Bar v2.0 (MY9221) | DI → GPIO18, DCKI → GPIO19 |
| Summer | Piezo LF-PB30W35B (aktiv, 9 V/9 mA) | GPIO26 (direkt oder über NPN) |
| Pumpen-Relais | Grove Relay, HLS8L-DC3V-S-C (3-V-Spule, 10 A/30 V DC) | SIG → GPIO27 |
| Pumpe | 12 V DC Membranpumpe 385 | über Relais, eigenes 12-V-Netzteil |
| Tank-Füllstand | **fehlt** | reserviert GPIO32 |
| Status-LED | Onboard | GPIO2 |

Noch offen für die Pumpe: 12-V-Netzteil (≥ 1,5 A), Freilaufdiode 1N4007, Schlauch, Wasserbehälter; Grove-auf-Jumper-Kabel für DHT, LED-Bar, Relais.

## Firmware (`firmware/`)

- PlatformIO, Arduino-Framework, Board `esp32dev`
- Bibliotheken: `adafruit/DHT sensor library`, `Seeed Grove LED Bar`, `bblanchon/ArduinoJson`, eingebautes `HTTPClient`/`WiFiClientSecure`
- `firmware/include/secrets.h.example` → lokal nach `secrets.h` kopieren (WLAN, Server-URL, API-Key; gitignored)

Funktionen:

1. WLAN mit Auto-Reconnect, Status auf Onboard-LED
2. Sensoren lesen: Median aus 5 Messungen, Kalibrierung roh → %, Fehlercodes; Bodensensor nur während der Messung versorgen
3. POST alle `interval_s` Sekunden nach Vertrag, Antwort auswerten (`commands`, `config`)
4. **Lokale Bewässerungslogik** mit harten Grenzen (max. Laufzeit pro Lauf, Pause, Tageslimit), funktioniert auch ohne Server; Relais beim Boot sicher AUS
5. LED-Bar zeigt Bodenfeuchte (0–10 Segmente) → ohne Handy lesbar (Barrierefreiheit)
6. Summer bei kritischen Zuständen, per Config abschaltbar
7. Serielles Log (115200 Baud) und Test-Modus pro Sensor
8. Phase 3: HTTPS mit hinterlegtem Zertifikat des Pi (Certificate Pinning)

## KI-Modul (`ai/`)

Reines Python (numpy, optional scikit-learn), läuft im Server-Prozess auf dem Pi.

1. **Wasserbedarf-Prognose:** Trend der Bodenfeuchte (Regression, Pumpenereignisse herausgerechnet) → Stunden bis `moisture_min_pct`, korrigiert nach Temperatur/Licht
2. **Anomalie-Erkennung:** robuster Z-Score (Median/MAD) + Sprungerkennung + „Sensor hängt“
3. **Pflanzenstress-Score** 0–100 mit Begründungen
4. **Wasserersparnis-Schätzung** gegenüber festem Gießplan (Nachhaltigkeit/CO₂)
5. Unit-Tests mit synthetischen Daten

## Tools (`tools/`)

- `fake_esp.py`: simuliert den ESP (Tagesverlauf, Austrocknen, Gießen, Anomalien), damit Claude-Web ohne Hardware testen kann

## Reihenfolge

| Phase | Inhalt |
|---|---|
| 0 | Plan, Vertrag, Verkabelung, `tools/fake_esp.py`, KI-Stub |
| 1 | Firmware: alle Sensoren lesen + seriell ausgeben, dann POST an Server |
| 2 | LED-Bar, Summer, Kalibrierung; KI v1 (Prognose + Anomalien) |
| 3 | Relais/Pumpe + lokale Bewässerungslogik; HTTPS/Pinning; Stress-Score |
| 4 | Tests, Hardware-Doku (Fotos), Beitrag zu Sicherheitsanalyse & Präsentation |

## Status

Erledigt:

- [x] Aufgabe gelesen, Hardware gesichtet
- [x] Plan, API-Vertrag v1.0, Regeln (`.claude/`)
- [x] Pinout + Verkabelungsplan (`docs/hardware/verkabelung.md`)

Als Nächstes:

- [ ] `tools/fake_esp.py`
- [ ] KI-Stub `ai/garden_ai.py`
- [ ] Firmware Phase 1 (Sensoren seriell)

Offene Fragen / Blocker:

- Netzwerk: gemeinsames WLAN für ESP32 und Pi? (Client-Isolation im Hackathon-WLAN?)
- Teile für die Pumpe (Netzteil, Schlauch, Behälter), Grove-Kabel
