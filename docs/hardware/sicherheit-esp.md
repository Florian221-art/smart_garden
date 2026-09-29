# Sicherheitsanalyse ESP32-Teil

Stand: 29.09.2026 · Firmware v0.3.0 · Gegenstück für Server/Dashboard: `docs/server/` (Nico)

Dieses Dokument ist der ESP-Teil der Bedrohungsmodellierung, die die Aufgabe fordert („Bedrohungsmodellierung (OWASP)“). Es beschreibt, was geschützt wird, wer angreifen könnte und welche Maßnahmen im ESP umgesetzt sind. Außerdem steht hier, welche Risiken bleiben.

## 1. Was schützen wir?

| Schutzgut | Warum wichtig |
|---|---|
| **Pumpe, Wasser, Pflanze** | Ein Aktor mit echter Wirkung. Läuft die Pumpe falsch, drohen Trockenlauf (Pumpe kaputt), Überschwemmung, Wasserverschwendung oder eine vertrocknete Pflanze. |
| **API-Key des Geräts** | Wer ihn hat, kann im Namen des Kübels falsche Messwerte einspeisen. |
| **WLAN-Passwort des Hotspots** | Wer es kennt, ist im selben Netz wie Pi und ESP. |
| **Messdaten** | Keine personenbezogenen Daten (keine Kamera, kein Mikrofon, keine Nutzerkennung), aber Grundlage für Dashboard und KI. |

## 2. Systemgrenzen und Datenflüsse

```
 [Sensoren] --analog/1-Wire--> [ESP32] --WLAN (WPA2), HTTP(S) + X-API-Key--> [Pi: FastAPI] <-- [Dashboard/Browser]
                                  |  ^                                             |
                        Relais -> Pumpe  +--- Antwort: config / commands / demo ---+
                                  ^
                     USB (serielle Konsole, Hochladen), BOOT-Taste
```

Vertrauensgrenzen:

1. **WLAN** zwischen ESP und Pi (Funk, prinzipiell mithörbar)
2. **Serverantwort**: Der ESP führt Befehle aus, die jemand im Dashboard ausgelöst hat.
3. **Physischer Zugang**: USB-Kabel und BOOT-Taste am Kübel

## 3. Bedrohungen (STRIDE) und Maßnahmen

| # | Bedrohung | STRIDE | Maßnahme im ESP | Restrisiko |
|---|---|---|---|---|
| T1 | Jemand schickt über das Dashboard oder einen gefälschten Server massenhaft Pump-Befehle | Tampering / Elevation | Alle Starts laufen durch `pumpStart()`: höchstens 15 s pro Lauf, **mindestens 10 s Pause vor jedem Start**, Tageslimit höchstens 300 s, Trockenlaufschutz. Die Grenzen sind einkompiliert und vom Server nicht änderbar. | Innerhalb der Grenzen kann gegossen werden. Die Admin-Anmeldung am Server ist Aufgabe von Server/Dashboard. |
| T2 | Manipulierte `config` (z. B. Tageslimit 10 000 s, Pause 0 s, Tank 50 l) | Tampering | `configClampSettings()` begrenzt **jeden** Wert, auch die aus dem Flash. NaN wird auf den Standardwert gesetzt. | Werte innerhalb der Bereiche sind möglich, z. B. hohe Zielfeuchte. Die Tankgröße begrenzt die Wassermenge physikalisch. |
| T3 | Trockenlaufschutz aushebeln über den Demo-Füllstand `water_level_pct: 100` | Tampering | Der **Demo-Füllstand ist nur Anzeige** und wird nicht gespeichert. Der Schutz nutzt `min(echt, Demo)`: Eine Demo kann den Tank nur leerer machen, nie voller. Nach der Demo gilt wieder die echte Schätzung. | – |
| T4 | Trockenlaufschutz aushebeln über den Befehl `tank_refilled` | Tampering | Der ESP kann nicht prüfen, ob wirklich aufgefüllt wurde. Jede Quelle wird geloggt (`[TANK] ... (Dashboard)`). Das Tageslimit begrenzt die Laufzeit. | **Bleibt bestehen** → der Server muss den Befehl hinter einer Admin-Anmeldung schützen (Issue an Web). |
| T5 | Demo-Werte außerhalb jeder Realität (z. B. Temperatur 1e9, NaN) | Tampering | Wertebereiche pro Feld (z. B. Temperatur −20…60 °C). NaN und Inf werden verworfen. Demos laufen höchstens 600 s. JSON-Ausgabe über `putNumber()`, daher nie `nan` im JSON. | – |
| T6 | Mitlesen des API-Keys im WLAN | Information Disclosure | WLAN mit WPA2. HTTPS ist vorbereitet (`https://`, mit `SERVER_CA_CERT` inklusive Zertifikatsprüfung). Der Key wird nie seriell ausgegeben. | Solange der Server nur HTTP spricht, kann ihn mitlesen, wer im WLAN ist. → Caddy mit TLS einrichten. |
| T7 | Gefälschter Server (gleiche SSID, eigener Pi) | Spoofing | Mit `SERVER_CA_CERT` akzeptiert der ESP nur den echten Server. | Ohne Zertifikat oder über HTTP bleibt das Risiko. Folgen begrenzen T1/T2. |
| T8 | Passwort oder API-Key landen im öffentlichen GitHub-Repo | Information Disclosure | Optional `secrets.h` (in `.gitignore`), Platzhalter im Sketch, Warnung beim Start. `WiFi.persistent(false)`: Die Zugangsdaten werden nicht zusätzlich im WLAN-Flash gespeichert. | Wer die Daten doch in den Sketch schreibt und committet, veröffentlicht sie → vor jedem Commit `git diff` prüfen. |
| T9 | Firmware hängt, Relais bleibt an | Denial of Service | Nicht blockierender Code, kein HTTP während die Pumpe läuft, alle Netzwerk-Timeouts 4 s (auch der TLS-Handshake). **Watchdog** nach 20 s: Neustart, `pumpBegin()` schaltet das Relais als Erstes aus. Nach jedem Start wartet das Auto-Gießen die Pause ab, damit eine Neustart-Schleife nicht dauernd pumpt. Zusätzlich wird bei jedem `loop()` geprüft, dass das Relais aus ist, wenn keine Pumpe laufen soll. | – |
| T10 | Sensor defekt, abgezogen oder falsch kalibriert → Dauergießen | (Sicherheit/Safety) | Plausibilitätsgrenzen für den Rohwert (100…4000). Ungültige Kalibrierung (trocken − nass < 300) wird nicht gespeichert und gilt als Sensorfehler. Bei Sensorfehler gibt es **kein** Auto-Gießen. | – |
| T11 | Sensor steckt nicht in der Erde, Schlauch liegt daneben → Wasser läuft ins Leere | (Safety / Nachhaltigkeit) | **Plausibilitätsprüfung**: Bringen 3 Läufe (in mindestens 3 min) weniger als +3 % Feuchte, sperrt der ESP das Auto-Gießen und gibt Alarm. | Die Prüfung setzt im Demo-Modus aus (Werte sind dort fest). |
| T12 | Angreifer mit physischem Zugang (USB, BOOT-Taste) | Elevation | Die serielle Konsole ist nur per Kabel erreichbar. Auch dort gelten alle Pumpengrenzen. | Mit physischem Zugang kann man neue Firmware aufspielen. Für einen Schulkübel akzeptiert. Gegenmaßnahme wären Secure Boot und Flash-Verschlüsselung. |
| T13 | Server liefert riesige oder kaputte Antworten | DoS | HTTP-Timeouts von 4 s. Kaputtes JSON wird verworfen, Fehlertexte werden auf 200 Zeichen gekürzt. Bei Speicherproblemen greift der Watchdog. | – |

## 4. Abgleich mit OWASP IoT Top 10 (2018)

| OWASP IoT | Umsetzung im ESP |
|---|---|
| I1 Schwache/voreingestellte Passwörter | Keine Standardpasswörter in der Firmware, nur Platzhalter mit Warnung. Den API-Key erzeugt der Server pro Gerät. |
| I2 Unsichere Netzwerkdienste | Der ESP bietet **keinen** Dienst an: kein Webserver, kein OTA, kein Telnet. Er baut nur ausgehende Verbindungen auf. |
| I3 Unsichere Schnittstellen im Ökosystem | Der API-Key wird bei jedem Request mitgeschickt. Die Absicherung der Dashboard-Befehle liegt beim Server (siehe T4). |
| I4 Fehlender sicherer Update-Mechanismus | Updates gibt es bewusst nur per USB. OTA wäre eine zusätzliche Angriffsfläche. |
| I5 Unsichere/veraltete Komponenten | Aktueller ESP32-Core 3.3.x, ArduinoJson 7, Adafruit DHT; keine Bibliotheken unklarer Herkunft (LED-Treiber selbst geschrieben). |
| I6 Unzureichender Datenschutz | Es werden nur Umweltdaten erhoben, keine personenbezogenen Daten. |
| I7 Unsichere Übertragung/Speicherung | Übertragung per WPA2, HTTPS ist vorbereitet. Im Flash stehen nur Einstellungen, Kalibrierung und der Tankstand, keine Zugangsdaten im WLAN-Speicher. |
| I8 Fehlende Geräteverwaltung | Geräte-ID und Firmware-Version stehen in jeder Nachricht. Der Server sieht `uptime_s`, `rssi_dbm` und Fehlercodes. |
| I9 Unsichere Standardeinstellungen | Die Standardwerte sind konservativ: 5 s pro Lauf, 300 s Pause, 60 s pro Tag. |
| I10 Fehlende physische Härtung | Siehe T12. Die Elektronik muss vor Spritzwasser geschützt werden (verkabelung.md). |

## 5. Empfehlungen an Server/Dashboard (aus dem Review)

Diese Punkte kann nur die Serverseite lösen. Sie stehen ausführlich im GitHub-Issue an Web.

1. **Admin-Anmeldung** für alle Steuer-Endpunkte (Pumpe, `tank_refilled`, Demo). Das ist die wichtigste Maßnahme, sie schließt T1 und T4.
2. **TLS** (Caddy `tls internal`) und das Root-Zertifikat für `SERVER_CA_CERT` bereitstellen. Das schließt T6 und T7.
3. Uvicorn nur im Hotspot erreichbar machen (an `10.42.0.1` binden oder per Firewall).
4. Ein Rate-Limit vor der Prüfung des API-Keys.

## 6. Review-Protokoll Firmware v0.2.4 → v0.3.0

| Befund in v0.2.4 | Schwere | Behoben in v0.3.0 |
|---|---|---|
| Demo-Füllstand wurde **dauerhaft** als echte Tank-Schätzung gespeichert. „Tank 100“ per Demo hob den Trockenlaufschutz auf, „Tank 4“ blockierte die Pumpe auch nach der Demo. | Hoch | Demo-Füllstand nur noch als Anzeige, Schutz mit `min(echt, Demo)` |
| Dashboard-, Demo- und serielle Pump-Befehle ignorierten jede Pause. Viele Befehle hintereinander ergaben Dauerbetrieb bis zum Tageslimit. | Hoch | Harte Mindestpause von 10 s vor jedem Start |
| Eine Fehlkalibrierung (trocken ≈ nass) ergab 0 % Feuchte und damit Dauergießen bis zum Tageslimit. | Hoch | Kalibrierung wird geprüft, ungültig = Sensorfehler = kein Gießen |
| Kein Schutz, wenn der Sensor nicht in der Erde steckt | Mittel | Plausibilitätsprüfung mit Sperre und Alarm |
| Kein Watchdog: Hing die Firmware bei laufender Pumpe, lief sie bis zum Neustart weiter. | Mittel | Task-Watchdog 20 s, Relais-Sicherheitsnetz in `pumpUpdate()` |
| `ledtest`/`ledswap` blockierten 3 s mit `delay()`, auch bei laufender Pumpe | Mittel | Testanimation läuft im Hintergrund |
| Demo-Temperatur ohne Grenzen. NaN hätte ungültiges JSON (`nan`) erzeugt. | Niedrig | Wertebereiche pro Feld, `putNumber()` schreibt `null` |
| `pump_cooldown_s` ohne Obergrenze: Überlauf bei `s * 1000` möglich | Niedrig | Grenze 86 400 s |
| Bodenrohwert ≥ 4000 (Kurzschluss nach 3V3) galt als gültig, als 0 %, und löste Gießen aus | Niedrig | Plausibilitätsbereich 100…4000 |
| WLAN-Zugangsdaten zusätzlich im WLAN-Flash gespeichert | Niedrig | `WiFi.persistent(false)` |
| Passwort nur im Sketch: Risiko, es ins öffentliche Repo zu committen | Niedrig | Optionale `secrets.h`, Warnung bei Platzhaltern |
| Unplausible DHT-Werte wurden übernommen | Niedrig | Bereichsprüfung |
| *Zweite Review-Runde (unabhängige Prüfung der Änderungen):* | | |
| Hängender TLS-Handshake (Standard-Timeout 120 s) hätte den Watchdog ausgelöst. In einer Neustart-Schleife wäre dann bei jedem Start sofort gegossen worden, und das Tageslimit lag nur im RAM. | Hoch | Handshake-Timeout 4 s. Nach jedem Start wartet das Auto-Gießen erst `pump_cooldown_s`. Grund des Neustarts wird geloggt. |
| Demo-Bodenwert konnte die Gieß-Sperre aufheben. Eine Demo-Sitzung lief mit echten Werten weiter, die Plausibilitätsprüfung rechnete dann mit dem Demo-Startwert. | Mittel | Sperre hebt nur ein echter Wert auf. Ein Wechsel zwischen Demo und echt beendet die Sitzung. |
| Demo-Sofortgießen ignorierte `auto_water = false` und die Gieß-Sperre | Mittel | Demo-Gießen nur bei aktivem Auto-Gießen ohne Sperre |
| `cal light` speicherte ein ungültiges Boden-Kalibrierpaar mit | Niedrig | Gespeichert wird nur, wenn das Bodenpaar gültig ist |
| `demo soil nan` (seriell) | Niedrig | Nicht-endliche Werte werden abgelehnt |
| Anzeige-Timer nach 24,8 Tagen Laufzeit falsch (Zeitstempel-Vergleich) | Niedrig | Flag + Startzeit statt Endzeit |
| Watchdog überwachte die WLAN-Idle-Task nicht mehr, Fehler bei der Einrichtung wurden nicht gemeldet | Niedrig | Idle-Task Core 0 wird mit überwacht, Status im Log (`[WDT]`) |

Geprüft und in Ordnung:

- Überlaufsichere `millis()`-Vergleiche
- Relais beim Start aus, bevor der Pin Ausgang wird
- Kein HTTP während die Pumpe läuft
- Kein Passwort und kein Key im Log
- Keine offenen Netzwerkdienste
- Serverwerte werden begrenzt
- Die Firmware kompiliert ohne Warnungen (`--warnings all`)
