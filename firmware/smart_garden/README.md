# ESP32-Firmware Smart Garden

C++ / Arduino-Framework. Verkabelung: [`docs/hardware/verkabelung.md`](../../docs/hardware/verkabelung.md)

## 1. Einmalig: Arduino IDE einrichten

1. **Arduino IDE 2.x** installieren.
2. *Datei → Einstellungen → Zusätzliche Boardverwalter-URLs:*
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
3. *Werkzeuge → Board → Boardverwalter:* **esp32 by Espressif Systems** installieren (Version **3.x**).
4. *Werkzeuge → Bibliotheken verwalten* – installieren:
   - **DHT sensor library** (Adafruit), Frage nach Abhängigkeiten mit „Alle installieren“ beantworten (→ Adafruit Unified Sensor)
   - **ArduinoJson** (Benoit Blanchon, Version 7.x)
   - Die Grove LED Bar braucht **keine** Bibliothek – der Treiber ist eingebaut (`ledbar.cpp`).
5. Treiber für den USB-Chip **CP2102** (Silicon Labs), falls Windows keinen COM-Port anzeigt.

## 2. Einstellungen eintragen

Ganz oben in **`smart_garden.ino`** im Block **„EINSTELLUNGEN – HIER ANPASSEN“**:

| Variable | Bedeutung |
|---|---|
| `CFG_WIFI_SSID` | WLAN-Name, `SmartGarden` (Hotspot des Pi) |
| `CFG_WIFI_PASSWORD` | WLAN-Passwort (von Nico) |
| `CFG_SERVER_URL` | `http://10.42.0.1:8000` |
| `CFG_API_KEY` | wird auf dem Server beim Anlegen des Geräts erzeugt |
| `CFG_DEVICE_ID` | `esp32-kuebel-01` |
| `CFG_LEDBAR_MODE` | `0` = Zeiger (2 LEDs an der Position: trocken = rot, feucht = grün) · `1` = Füllbalken ab Rot |
| `CFG_LEDBAR_REVERSE` | `true`, falls die Anzeige gespiegelt erscheint (grün bei trockener Erde) |
| `CFG_BUZZER_ENABLED` | `false` = Summer komplett stumm |

**Das Repo ist öffentlich:** echtes Passwort und API-Key nicht committen. Vor einem `git pull` die lokale Änderung mit `git stash` beiseitelegen und danach mit `git stash pop` zurückholen.

Ohne gültiges WLAN funktionieren Sensoren, LED-Bar, Pumpe und Demo-Befehle trotzdem – nur das Senden an den Server nicht.

## 3. Hochladen

1. `firmware/smart_garden/smart_garden.ino` öffnen (alle anderen Dateien erscheinen als Tabs).
2. *Werkzeuge → Board:* **ESP32 Dev Module** · *Port:* COM-Port des ESP32
3. **Hochladen**. Falls „Connecting…“ hängen bleibt: **BOOT**-Taste gedrückt halten, bis der Upload startet.
4. *Werkzeuge → Serieller Monitor*, **115200 Baud**, Zeilenende **„Neue Zeile“**.

Beim Start: LED-Bar läuft einmal von Segment 1 (rot) bis 10 (grün) durch, der Summer piept kurz.
Leuchtet dabei zuerst Grün → oben `CFG_LEDBAR_REVERSE = true` setzen (zum Ausprobieren ohne Hochladen: `ledflip`).

## 4. Kalibrieren (einmal, dauert 2 Minuten)

| Schritt | Befehl |
|---|---|
| Bodensensor trocken an der Luft halten | `cal soil dry` |
| Bodensensor bis zur weißen Linie in ein Glas Wasser | `cal soil wet` |
| Lichtsensor mit dem Finger abdecken | `cal light dark` |
| Handy-Taschenlampe direkt auf den Lichtsensor | `cal light bright` |

Werte werden im ESP gespeichert und überleben Neustarts. Kontrolle mit `status`.

Pumpen-Durchfluss messen: Schlauch in einen Messbecher, `pump 10` eingeben, Menge durch 10 teilen → im Dashboard unter „Durchfluss“ eintragen.

## 5. Befehle im seriellen Monitor

| Befehl | Wirkung |
|---|---|
| `help` | Übersicht |
| `status` | aktuelle Werte, Einstellungen, Kalibrierung, Demo-Status |
| `demo soil 12` | Bodenfeuchte 12 % vortäuschen → LED-Bar rot, Pumpe startet (120 s, optional Sekunden als 3. Wert) |
| `demo temp 38` / `demo hum 25` / `demo light 2` | Temperatur / Luftfeuchte / Licht vortäuschen |
| `demo tank 4` | Tank fast leer → Summer, keine Bewässerung |
| `demo error dht` | Sensorausfall DHT11 (auch `soil`, `light`) |
| `demo off` | zurück zu echten Werten |
| `pump 3` | Pumpe 3 s laufen lassen (Sicherheitsgrenzen gelten) |
| `stop` | Pumpe sofort aus |
| `refill` | Tank als aufgefüllt markieren (oder BOOT-Taste 3 s halten) |
| `ledtest` / `ledflip` | LED-Bar testen / Richtung umdrehen (bis Neustart) |
| `ledseg 3` | genau 3 Segmente (ab Segment 1) 10 s lang anzeigen – zum Prüfen der Richtung |
| `beep` | Summer testen (piept auch, wenn stumm) |
| `mute` | Summer an/aus (bis Neustart) |
| `send` | sofort an den Server senden |

Der Demo-Modus lässt sich im Dashboard auch per Knopf auslösen (Admin). Demo-Werte werden an den Server als `demo_overrides` gemeldet.

## 6. Was die Firmware macht

- Misst alle 2 s: Bodenfeuchte (kapazitiv, GPIO34), Licht (GPIO35), Temperatur/Luftfeuchte (DHT11, GPIO4)
- LED-Bar: Bodenfeuchte 0–100 % → Position 1–10 (rot = trocken, grün = feucht), als Zeiger (Standard) oder Füllbalken; < 10 % blinkt rot; Tank leer → Segment 1 blinkt schnell; Sensorfehler → Segment 1/10 abwechselnd
- Summer: piept nur, wenn ein Problem **neu** auftritt (Tank leer, Sensorfehler), danach höchstens alle 10 min; Grund steht im seriellen Monitor als `[ALARM] …`
- **Tank leer (geschätzt) sperrt die Pumpe** (Trockenlaufschutz). Nach dem Auffüllen `refill` eingeben oder BOOT 3 s halten.
- Automatisches Gießen: unter `moisture_min_pct` startet eine Gieß-Sitzung, Pumpe läuft stoßweise (`max_pump_s_per_run`, Pause `pump_cooldown_s`) bis `moisture_target_pct` erreicht ist
- **Sicherheitsgrenzen** (auch im Demo-Modus): max. 15 s pro Lauf, Tageslimit, Pause, kein Pumpen bei Tank ≤ 5 %, Relais beim Start aus, kein HTTP-Request während die Pumpe läuft
- Tank-Schätzung aus Pumpenlaufzeit × Durchfluss, im Flash gespeichert
- Sendet alle `interval_s` Sekunden (Standard 15) an `POST /api/v1/readings`, übernimmt `config`, `commands` und `demo` aus der Antwort
- Onboard-LED: an = WLAN ok, langsam blinkend = verbindet, schnell blinkend = Senden fehlgeschlagen
