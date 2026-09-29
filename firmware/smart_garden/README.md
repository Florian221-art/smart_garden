# ESP32-Firmware Smart Garden

C++ mit dem Arduino-Framework (ESP32-Core 3.x). Hochgeladen wird mit der Arduino IDE.

Die Firmware misst Bodenfeuchte, Licht, Temperatur und Luftfeuchte. Sie zeigt die Bodenfeuchte auf der LED-Bar an, gießt selbstständig über Relais und Pumpe und schickt alle Werte an den Server auf dem Raspberry Pi. Ohne WLAN oder Server läuft alles außer dem Senden weiter.

| Weitere Doku | Inhalt |
|---|---|
| [`docs/hardware/verkabelung.md`](../../docs/hardware/verkabelung.md) | Pinout, Verkabelung, Inbetriebnahme |
| [`docs/hardware/sicherheit-esp.md`](../../docs/hardware/sicherheit-esp.md) | Bedrohungsmodell und Schutzmaßnahmen des ESP-Teils |
| [`.claude/api-contract.md`](../../.claude/api-contract.md) | Schnittstelle zum Server (JSON-Felder, Demo-Modus) |
| [`tools/`](../../tools/README.md) | `fake_esp.py`: simuliert diesen ESP, zum Testen des Servers ohne Hardware |

---

## 1. Einmalig: Arduino IDE einrichten

1. **Arduino IDE 2.x** installieren.
2. Unter *Datei → Einstellungen → Zusätzliche Boardverwalter-URLs* eintragen:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
3. Unter *Werkzeuge → Board → Boardverwalter* **esp32 by Espressif Systems** installieren, Version **3.x** (getestet mit 3.3.12).
4. Unter *Werkzeuge → Bibliotheken verwalten* installieren:
   - **DHT sensor library** (Adafruit). Die Frage nach Abhängigkeiten mit „Alle installieren“ beantworten, das bringt Adafruit Unified Sensor mit.
   - **ArduinoJson** von Benoit Blanchon, Version **7.x**
   - Die Grove LED Bar braucht **keine** Bibliothek, der Treiber ist in `ledbar.cpp` eingebaut.
5. Zeigt Windows keinen COM-Port an: Treiber für den USB-Chip **CP2102** (Silicon Labs) installieren.

## 2. Einstellungen eintragen

Die Einstellungen stehen ganz oben in **`smart_garden.ino`** im Block **„EINSTELLUNGEN – HIER ANPASSEN“**:

| Variable | Bedeutung |
|---|---|
| `CFG_WIFI_SSID` | WLAN-Name, `SmartGarden` (Hotspot des Pi) |
| `CFG_WIFI_PASSWORD` | WLAN-Passwort (von Nico) |
| `CFG_SERVER_URL` | `http://10.42.0.1:8000`, später `https://10.42.0.1` |
| `CFG_API_KEY` | wird auf dem Server beim Anlegen des Geräts erzeugt |
| `CFG_DEVICE_ID` | `esp32-kuebel-01`, muss zum Gerät auf dem Server passen |
| `CFG_LEDBAR_MODE` | `2` = Trockenheitsbalken ab Grün (Standard), `0` = Zeiger, `1` = Füllbalken ab Rot (Abschnitt 6) |
| `CFG_LEDBAR_REVERSE` | `true`, falls die Anzeige gespiegelt erscheint |
| `CFG_LEDBAR_SWAP_PINS` | `true`, falls die LED-Bar gar nicht reagiert (Takt/Daten vertauscht) |
| `CFG_BUZZER_ENABLED` | `false` schaltet den Summer komplett stumm |

### Sicherer: Zugangsdaten in `secrets.h`

Das Repo ist **öffentlich**. Ein echtes Passwort oben im Sketch kann aus Versehen mitcommittet werden. So vermeidest du das:

1. `secrets.h.example` im selben Ordner kopieren und in **`secrets.h`** umbenennen.
2. Passwort und API-Key dort eintragen.
3. Die Arduino IDE neu öffnen und hochladen.

`secrets.h` steht in `.gitignore` und wird deshalb nie hochgeladen. Ihre Werte haben Vorrang vor dem Sketch. Beim Start erscheint dann `[CFG] Zugangsdaten aus secrets.h übernommen`.

Wenn du doch direkt im Sketch arbeitest: vor jedem `git pull` erst `git stash` ausführen, danach `git stash pop`. Vor einem Commit mit `git diff` prüfen, dass kein Passwort drinsteht.

Stehen noch die Platzhalter drin, meldet die Firmware beim Start `WARNUNG: ... Platzhalter`. Sensoren, LED-Bar, Pumpe und Demo-Befehle funktionieren trotzdem, nur das Senden nicht.

## 3. Hochladen

1. `firmware/smart_garden/smart_garden.ino` öffnen. Alle anderen Dateien erscheinen als Tabs.
2. Unter *Werkzeuge → Board* **ESP32 Dev Module** wählen, unter *Port* den COM-Port des ESP32.
3. Auf **Hochladen** klicken.
4. *Werkzeuge → Serieller Monitor* öffnen, **115200 Baud**, Zeilenende **„Neue Zeile“**.

Beim Start steht im Monitor `Firmware 0.3.1`. Die LED-Bar füllt sich einmal von Grün über Orange bis Rot, und der Summer piept kurz. Leuchtet dabei zuerst Rot, oben `CFG_LEDBAR_REVERSE = true` setzen (zum Ausprobieren ohne Hochladen: `ledflip`).

**Wenn der Upload klemmt** (`Connecting…` hängt oder `Wrong boot mode detected`):

- **BOOT** gedrückt halten, bis der Upload startet, notfalls dabei kurz **EN** drücken.
- Während des Uploads die 3V3-Schiene und das 12-V-Netzteil abziehen.
- Unter *Werkzeuge → Upload Speed* **115200** einstellen.

## 4. Kalibrieren (einmal, ca. 2 Minuten)

| Schritt | Befehl |
|---|---|
| Bodensensor trocken an der Luft halten | `cal soil dry` |
| Bodensensor bis zur weißen Linie in ein Glas Wasser stecken | `cal soil wet` |
| Lichtsensor mit dem Finger abdecken | `cal light dark` |
| Handy-Taschenlampe direkt auf den Lichtsensor halten | `cal light bright` |

Die Werte bleiben im ESP gespeichert, auch nach Neustart und neuem Hochladen. Kontrolle mit `status`.

Beim Bodensensor muss der Rohwert „trocken“ mindestens **300** über „nass“ liegen, sonst wird nicht gespeichert (typisch: trocken ~3200, nass ~1100). Ist die Kalibrierung ungültig, meldet der Sensor einen Fehler und es wird nicht gegossen. So verhindert die Firmware, dass eine Fehlkalibrierung als „0 % Feuchte“ gelesen wird und Dauergießen auslöst.

**Pumpen-Durchfluss messen:** Schlauch in einen Messbecher halten, `pump 10` eingeben und die Menge durch 10 teilen. Das Ergebnis ist der Durchfluss in ml/s, der im Dashboard unter „Durchfluss“ eingetragen wird. Nur mit diesem Wert stimmt die Tank-Schätzung.

## 5. Befehle im seriellen Monitor

| Befehl | Wirkung |
|---|---|
| `help` | Übersicht |
| `status` | aktuelle Werte, Einstellungen, Kalibrierung, Demo-Status (ohne Passwort/Key) |
| `demo soil 12` | Bodenfeuchte 12 % vortäuschen: LED-Bar voll, Pumpe startet. Gilt 120 s, optional die Sekunden als 3. Wert, maximal 600 s |
| `demo temp 38` / `demo hum 25` / `demo light 2` | Temperatur / Luftfeuchte / Licht vortäuschen |
| `demo tank 4` | Tank fast leer vortäuschen: Summer, keine Bewässerung. Die echte Tank-Schätzung bleibt unverändert |
| `demo error dht` | Sensorausfall DHT11 simulieren (auch `soil`, `light`) |
| `demo off` | zurück zu echten Werten |
| `pump 3` | Pumpe 3 s laufen lassen (Sicherheitsgrenzen gelten) |
| `stop` | Pumpe sofort aus |
| `refill` | Tank als aufgefüllt markieren (oder BOOT-Taste 3 s halten). Hebt auch die Gieß-Sperre auf |
| `cal soil dry\|wet`, `cal light dark\|bright` | Kalibrieren (Abschnitt 4) |
| `ledtest` / `ledflip` | LED-Bar testen / Richtung umdrehen (bis Neustart) |
| `ledseg 3` | genau 3 Segmente (ab Segment 1 = rot) 10 s lang anzeigen |
| `ledswap` | Takt- und Datenpin tauschen (bis Neustart), falls die Bar nicht reagiert |
| `beep` | Summer testen (piept auch, wenn stumm geschaltet) |
| `mute` | Summer an/aus (bis Neustart) |
| `send` | sofort an den Server senden |

Den Demo-Modus kann man auch im Dashboard per Knopf auslösen. Demo-Werte meldet die Firmware dem Server im Feld `demo_overrides`, damit sie im Dashboard markiert und nicht fürs KI-Training verwendet werden.

## 6. Verhalten

### Messen und Anzeigen

- Alle 2 s werden Bodenfeuchte (GPIO34), Licht (GPIO35) und Temperatur/Luftfeuchte (DHT11 an GPIO4, höchstens alle 5 s) gemessen. Für jeden Analogwert nimmt die Firmware den Median aus 5 Messungen.
- **LED-Bar** im Standardmodus `2`: Je trockener die Erde, desto mehr LEDs leuchten.

  | Bodenfeuchte | LEDs |
  |---|---|
  | 100 % | 1 grüne |
  | 90 % | 2 grüne |
  | 80 % | 3 grüne |
  | 70 % | 4 grüne |
  | 60 % | 5 grüne |
  | 50 % | 6 grüne |
  | 40 % | 7 grüne |
  | 30 % | 8 grüne |
  | 11–20 % | 8 grüne + orange |
  | 10 % und weniger | alle 10 (8 grüne + orange + rot) |

  Sonderanzeigen: Bei einem Sensorfehler leuchten Segment 1 und 10 abwechselnd. Bei leerem Tank blinkt Segment 1 schnell. Der Dashboard-Befehl „identify“ lässt 5 s lang ein Lauflicht laufen.
- **Onboard-LED**: an = WLAN ok, langsam blinkend = verbindet, schnell blinkend = Senden fehlgeschlagen.

### Gießen

- Nach dem Einschalten wartet das Auto-Gießen erst einmal `pump_cooldown_s` (Standard 5 min). Das ist ein Schutz, falls der ESP immer wieder neu startet. Demo und manuelle Befehle gehen nach 10 s.
- Liegt die Feuchte unter `moisture_min_pct` (Standard 30 %), beginnt eine Gieß-Sitzung. Die Pumpe läuft stoßweise, je `max_pump_s_per_run` (5 s), mit `pump_cooldown_s` (300 s) Pause dazwischen, damit das Wasser versickert. Das geht so weiter, bis `moisture_target_pct` (55 %) erreicht ist.
- **Tank-Schätzung** ohne Sensor: Restmenge = Kapazität − Laufzeit × Durchfluss. Sie liegt im Flash und bleibt bei Neustart erhalten. Nach dem Auffüllen `refill` eingeben, die BOOT-Taste 3 s halten oder im Dashboard „Tank aufgefüllt“ drücken.

### Sicherheit

- **Sicherheitsgrenzen**, fest einkompiliert und vom Server nicht änderbar:
  - höchstens 15 s pro Lauf
  - Tageslimit höchstens 300 s
  - mindestens 10 s Pause vor jedem Start, auch bei Dashboard- und Demo-Befehlen
  - kein Pumpen bei Tank ≤ 5 %
- **Plausibilitätsprüfung**: Steigt die Feuchte nach 3 Läufen (und mindestens 3 min) um weniger als 3 Prozentpunkte, sperrt die Firmware das Auto-Gießen und gibt Alarm. Mögliche Ursachen: Sensor nicht in der Erde, Schlauch daneben, Pumpe saugt Luft. Aufgehoben wird die Sperre durch `refill`, die BOOT-Taste oder wenn die Feuchte wieder steigt.
- **Watchdog**: Hängt `loop()` länger als 20 s, startet der ESP neu. Das Relais geht dabei als Erstes aus. Beim Start steht der Grund des letzten Neustarts im Monitor (`[BOOT] ...`).
- Während die Pumpe läuft, wird nicht gesendet. Ein langsamer Server kann das Ausschalten also nicht verzögern.

### Alarm und Server

- **Summer**: piept nur, wenn ein Problem **neu** auftritt, danach höchstens alle 10 min. Probleme sind leerer Tank, Sensorfehler (erst nach 60 s) und wirkungsloses Gießen. Der Grund steht im Monitor als `[ALARM] …`.
- **Server**: Die Firmware sendet alle `interval_s` Sekunden (Standard 10) an `POST /api/v1/readings` und übernimmt `config`, `commands` und `demo` aus der Antwort.

## 7. Aufbau des Codes

| Datei | Aufgabe |
|---|---|
| `smart_garden.ino` | Einstellungen (oben), `setup()`/`loop()`, Messzyklus, Gieß-Logik mit Plausibilitätsprüfung, JSON bauen und senden, Serverantwort auswerten, serielle Konsole, Watchdog |
| `garden_config.h/.cpp` | Pins, **harte Sicherheitsgrenzen**, Settings und Kalibrierung im Flash (NVS), Begrenzung aller Serverwerte, `secrets.h` einlesen |
| `sensors.h/.cpp` | Bodenfeuchte (kapazitiv), LDR, DHT11; Median-Filter, Plausibilitätsprüfung, Kalibrierung anwenden |
| `pump.h/.cpp` | Relais nicht blockierend; jede Prüfung vor dem Einschalten (Tank, Pause, Laufzeit, Tageslimit) |
| `tank.h/.cpp` | Tank-Schätzung im Flash; Demo-Füllstand als reine Anzeige (kann den Tank nur leerer machen) |
| `demo.h/.cpp` | Demo-Modus: Overrides mit Wertebereichen, erzwungene Fehler, Ablauf-Timer |
| `display.h/.cpp` | LED-Bar-Anzeige und Testanimation, Summer mit Alarm-Logik, Status-LED; alles nicht blockierend |
| `ledbar.h/.cpp` | eigener Treiber für den MY9221-Chip der Grove LED Bar |
| `garden_net.h/.cpp` | WLAN mit Reconnect, HTTP(S)-POST, JSON-Antwort parsen, optional Zertifikatsprüfung |
| `secrets.h.example` | Vorlage für die optionale `secrets.h` |

**Hinweise für Änderungen:**

- Die Dateinamen `garden_net.*` und `garden_config.*` sind bewusst so gewählt. `Network.h` und `config.h` kollidieren mit Dateien im ESP32-Core, und Windows unterscheidet keine Groß-/Kleinschreibung.
- Die Vorwärtsdeklarationen oben im `.ino` müssen bleiben: Die automatische Prototyp-Erzeugung der Arduino IDE ist unzuverlässig. Jede neue `static`-Funktion im `.ino` dort eintragen.
- Nichts Blockierendes in `loop()` einbauen, also kein `delay()` über wenige ms. Die Pumpe wird über `millis()` ausgeschaltet.
- Neue Felder in `Settings` oder `Calibration` machen den gespeicherten Stand ungültig, der ESP startet dann mit den Standardwerten. Danach neu kalibrieren.
- Kompiliertest ohne Hardware: `arduino-cli compile --fqbn espressif:esp32:esp32 --warnings all firmware/smart_garden`. Die Firmware muss ohne Warnungen kompilieren.

## 8. Bekannte Einschränkungen

- **Kein Füllstandssensor**: Die Tank-Schätzung stimmt nur, wenn der Durchfluss gemessen und nach jedem Auffüllen `refill` ausgelöst wurde.
- **Keine Uhr**: Das Tageslimit gilt für jeweils 24 h Laufzeit. Nach einem Neustart beginnt das Fenster neu.
- **Sicherheit von HTTP**: Über HTTP ist der API-Key im WLAN mitlesbar. HTTPS mit `SERVER_CA_CERT` ist vorbereitet, sobald Caddy auf dem Pi läuft.
- **Server-Befehle**: Der ESP kann nicht prüfen, *wer* im Dashboard einen Befehl ausgelöst hat. Das muss der Server absichern (Admin-Login). Die harten Grenzen oben begrenzen den möglichen Schaden.
