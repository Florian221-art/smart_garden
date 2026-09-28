# Plan Claude-ESP (Florian)

Zuständig für: **ESP32-Firmware**, **KI-Modul** (`ai/`), **Test-Tools** (`tools/`), Hardware-Doku.
Schnittstelle zum Server: `.claude/api-contract.md`.

## Hardware (Stand 28.09.)

| Teil | Typ | Anschluss |
|---|---|---|
| Mikrocontroller | ESP32 DevKit (ESP-WROOM-32, CP2102, 30 Pin) | USB |
| Bodenfeuchte | IDUINO resistiv (S/+/−) | S → **GPIO34**, + → **GPIO25** (nur beim Messen einschalten, gegen Korrosion) |
| Licht | MH-Sensor LDR-Modul (VCC/GND/DO/AO) | AO → **GPIO35**, VCC → 3V3 (DO ungenutzt) |
| Temp./Luftfeuchte | Grove Temperature&Humidity v1.2 (= **DHT11**) | SIG → **GPIO4**, VCC → 3V3 |
| Anzeige | Grove LED Bar v2.0 (MY9221, 10 Segmente) | DI → **GPIO18**, DCKI → **GPIO19**, VCC → 3V3 |
| Summer | Piezo LF-PB30W35B (aktiv, 9 V/9 mA) | über NPN-Transistor an **GPIO26**, Versorgung VIN (5 V) |
| Pumpe | 12 V DC Membranpumpe 385 | über MOSFET/Relais an **GPIO27**, eigenes 12-V-Netzteil |
| Tank-Füllstand | **fehlt noch** | reserviert: **GPIO32** (ADC1) |
| Status-LED | Onboard-LED | GPIO2 |

Regeln: Analogsensoren nur an ADC1 (GPIO32–39), weil ADC2 mit WLAN nicht nutzbar ist. Strapping-Pins (0, 2 als Eingang, 12, 15) nicht für Sensoren.

### Noch zu besorgen (für die Pumpe)

- Logic-Level-MOSFET-Modul (z. B. IRLZ44N) **oder** Relaismodul mit 3,3-V-Ansteuerung
- 12-V-Netzteil ≥ 1,5 A, Freilaufdiode (1N4007) parallel zur Pumpe
- Schlauch, Wasserbehälter; optional Füllstandssensor oder Schwimmerschalter
- NPN-Transistor (2N2222/BC547) + 1-kΩ-Widerstand für den Summer
- Gemeinsame Masse (GND) zwischen 12-V-Kreis und ESP

## Firmware (`firmware/`)

- PlatformIO, Arduino-Framework, Board `esp32dev`
- Bibliotheken: `adafruit/DHT sensor library`, `Seeed Grove LED Bar`, `bblanchon/ArduinoJson`, eingebautes `HTTPClient`/`WiFiClientSecure`
- `firmware/include/secrets.h.example` → lokal kopieren nach `secrets.h` (WLAN, Server-URL, API-Key; gitignored)

Funktionen:

1. WLAN mit Auto-Reconnect, Status auf Onboard-LED
2. Sensoren lesen: Median aus 5 Messungen, Kalibrierung roh → %, Fehlercodes
3. POST alle `interval_s` Sekunden nach Vertrag, Antwort auswerten (`commands`, `config`)
4. **Lokale Bewässerungslogik** mit harten Sicherheitsgrenzen (max. Laufzeit pro Lauf, Pause, Tageslimit) – auch ohne Server
5. LED-Bar zeigt Bodenfeuchte (0–10 Segmente) → Status ohne Handy/Display lesbar (Barrierefreiheit)
6. Summer bei kritischen Zuständen (Sensorfehler, Tank leer), abschaltbar per Config
7. Serielles Log (115200 Baud) für Demo und Fehlersuche
8. Phase 3: HTTPS mit fest hinterlegtem Zertifikat des Pi (Certificate Pinning)

## KI-Modul (`ai/`)

Reines Python (numpy, optional scikit-learn), läuft im Server-Prozess auf dem Pi.

1. **Wasserbedarf-Prognose:** Trend der Bodenfeuchte (lineare Regression über die letzten Stunden, Pumpenereignisse herausgerechnet) → Stunden bis `moisture_min_pct`; Korrektur nach Temperatur/Licht
2. **Anomalie-Erkennung:** robuster Z-Score (Median/MAD) pro Messgröße + Sprungerkennung (z. B. plötzlicher Temperaturanstieg), Sensor-hängt-Erkennung (Wert ändert sich nicht)
3. **Pflanzenstress-Score** 0–100 aus Feuchte, Temperatur, Licht, Luftfeuchte mit Begründungen
4. **Wasserersparnis-Schätzung** gegenüber festem Gießplan (für Nachhaltigkeit/CO₂-Anzeige)
5. Unit-Tests mit synthetischen Daten (`ai/tests/`)

## Tools (`tools/`)

- `fake_esp.py`: simuliert den ESP (realistischer Tagesverlauf, Austrocknen, Gießen, Anomalien) – damit Claude-Web ohne Hardware entwickeln kann

## Reihenfolge

| Phase | Inhalt |
|---|---|
| 0 | Plan + Vertrag (dieser PR), `tools/fake_esp.py`, KI-Stub |
| 1 | Firmware: alle Sensoren lesen + seriell ausgeben, dann POST an Server |
| 2 | LED-Bar, Summer, Kalibrierung; KI v1 (Prognose + Anomalien) |
| 3 | Pumpe + lokale Bewässerungslogik; HTTPS/Pinning; KI-Stress-Score |
| 4 | Tests, Hardware-Doku (Schaltplan, Fotos), Beitrag zu Sicherheitsanalyse & Präsentation |

## Status

- [x] Plan und API-Vertrag v1.0
- [ ] `tools/fake_esp.py`
- [ ] KI-Stub `ai/garden_ai.py`
- [ ] Firmware Phase 1
