# Plan Claude-ESP (Florian)

Zuständig für: **ESP32-Firmware (C++)**, **Test-Tools** (`tools/`), **Hardware-Doku** (`docs/hardware/`), Beiträge zu Sicherheitsanalyse und Präsentation (ESP-Teil), Root-`README.md`.
Nicht zuständig: KI-Modul, Server, Dashboard (→ Claude-Web).
Schnittstelle zum Server: `.claude/api-contract.md` (v1.2) · Regeln: `.claude/CLAUDE.md`
Pinout + Verkabelung: **`docs/hardware/verkabelung.md`** · Firmware-Anleitung: **`firmware/smart_garden/README.md`** · Sicherheit: **`docs/hardware/sicherheit-esp.md`**

## Rahmenbedingungen (mit Florian geklärt)

- Firmware in **C++** (Arduino-Framework, ESP32-Core 3.x), geflasht mit der **Arduino IDE**. Florian lädt nur hoch → muss ohne Anpassungen kompilieren.
- **Zugangsdaten als Variablen ganz oben im Sketch** (Florians Wunsch); optional `secrets.h` (gitignored, hat Vorrang) – empfohlen, weil das Repo öffentlich ist.
- **Netzwerk:** Pi-Hotspot `SmartGarden`, Server `10.42.0.1`
- **Kein Füllstandssensor:** Tank wird über Pumpenlaufzeit geschätzt (Vertrag Abschnitt 3)
- **LED-Bar = Bodenfeuchte.** Florians Wunsch (29.09.): „erst alle grünen, dann orange, ganz trocken auch rot“ → Modus 2 „Trockenheitsbalken ab Grün“ ist Standard.
- **Demo-Modus** ist Pflicht (Vertrag Abschnitt 4)
- Kapazitiver Bodensensor (HW-390), dauerhaft an 3V3
- HTTP während der Entwicklung, HTTPS mit Zertifikatsprüfung vorbereitet (`SERVER_CA_CERT` in `secrets.h`)

## Hardware

| Teil | Typ | ESP32-Pin |
|---|---|---|
| Mikrocontroller | ESP32 DevKit V1 (ESP-WROOM-32, CP2102, 30 Pin) | – |
| Bodenfeuchte | Capacitive Soil Moisture Sensor v2.0 (HW-390) | AOUT → GPIO34 |
| Licht | LDR-Modul | AO → GPIO35 |
| Temp./Luftfeuchte | Grove v1.2 (DHT11) | SIG → GPIO4 |
| Anzeige | Grove LED Bar v2.0 (MY9221) | DCKI/Takt (gelb) → GPIO18, DI/Daten (weiß) → GPIO19 |
| Summer | aktiv, LF-PB30W35B | GPIO26 |
| Pumpen-Relais | Grove Relay | SIG → GPIO27 |
| Pumpe | 12 V DC | über Relais-Schraubklemme |
| Taster „Tank voll“ | BOOT-Taste onboard | GPIO0 |
| Status-LED | onboard | GPIO2 |

## Firmware-Versionen

| Version | Inhalt |
|---|---|
| 0.1.0 | komplette Firmware (Sensoren, LED-Bar, Summer, Demo, Pumpe, Tank, POST) |
| 0.2.0 | WLAN-Variablen ganz oben, klarere Logs, eigener LED-Treiber |
| 0.2.1 | DHT11 stabil (5 s, 30 s Hold), Alarm-Hysterese, Demo-Gießen ohne Cooldown |
| 0.2.2/0.2.3 | LED-Bar-Pins tauschbar, korrigiert (gelb = Takt D18, weiß = Daten D19) |
| 0.2.4 | LED-Modus 2 „Trockenheitsbalken ab Grün“ als Standard |
| **0.3.0** | **Review:** Demo-Tank nur Anzeige (min(echt, Demo)), harte 10-s-Mindestpause für alle Starts, Kalibrier-Prüfung, Plausibilitätsprüfung Gießen (Sperre + Alarm), Task-Watchdog 20 s, Relais-Sicherheitsnetz, nicht blockierender LED-Test, Wertebereiche Demo, `putNumber()` (kein `nan` im JSON), `WiFi.persistent(false)`, optionale `secrets.h`, Code und Doku vollständig kommentiert |

Hinweise für spätere Änderungen:

- Dateinamen `garden_net.*` / `garden_config.*` bewusst so gewählt (Kollision mit ESP32-Core unter Windows).
- Vorwärtsdeklarationen oben im `.ino` beibehalten und für jede neue Funktion ergänzen (exuberant ctags der IDE erzeugt sonst falsche Prototypen).
- Kompiliertest in der Cloud: `/tmp/bin/arduino-cli compile --fqbn espressif:esp32:esp32 --warnings all .` (Core manuell unter `/root/Arduino/hardware/espressif/esp32`, Libs per `git clone`). Muss ohne Warnungen durchlaufen.
- Florian pusht nicht selbst: Claude-ESP pusht über GitHub-MCP (`push_files`) in `feature/esp-*`, erstellt PR und merged nach Florians Freigabe („alles was du machst muss sinnvoll im GitHub liegen“).

## Status

Erledigt:

- [x] Plan, API-Vertrag v1.2, Regeln (`.claude/`)
- [x] Pinout + Verkabelung, Sicherheitsanalyse ESP-Teil (`docs/hardware/sicherheit-esp.md`)
- [x] `tools/fake_esp.py` + `tools/README.md` (auf Stand 0.3.0-Logik)
- [x] Firmware 0.3.0, kompiliert ohne Warnungen; Hardware läuft (Sensoren, LED-Bar, Summer, Pumpe) mit 0.2.4
- [x] Kalibrierung Boden (trocken 3248 / nass 1102)
- [x] Code-Review 29.09. (ESP selbst, Server/Web per Subagent read-only) → Befunde Server als Issue `an-web`
- [x] Root-README

Als Nächstes:

- [ ] Florian: 0.3.0 hochladen, Kurztest (`status`, `demo soil 12`, `demo tank 4` → danach wieder echter Tank, `pump 3` zweimal schnell → 2. blockiert)
- [ ] Durchfluss messen (`pump 10` in Messbecher) → `pump_flow_ml_per_s`
- [ ] Test gegen Nicos Server (WLAN-Passwort + API-Key in `secrets.h`)
- [ ] HTTPS: Caddy-Root-Zertifikat als `SERVER_CA_CERT`
- [ ] [CONTRACT]-Vorschlag: Fehlercode `watering_ineffective`, Demo-Tank als reine Anzeige im Vertrag beschreiben (PR offen, braucht Zustimmung beider)
- [ ] Fotos/Schaltplan, Präsentationsbeitrag ESP

Blocker:

- WLAN-Passwort des Pi-Hotspots + API-Key (kommt von Nico)
