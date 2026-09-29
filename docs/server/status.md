# Claude-Web – Fortschritt & Architektur (Stand: siehe Datum unten)

Diese Datei dokumentiert für Menschen (Nico, Florian), was im Bereich
Backend/Dashboard/Deployment bisher gebaut und getestet wurde. Der laufend
gepflegte Kurzstatus (fertig/als Nächstes/Blocker) steht wie vereinbart in
`.claude/plan-webserver.md`; hier steht die ausführlichere Erklärung dazu.

Letzte Aktualisierung: 2026-09-28

## Was schon läuft (Phase 1)

### 1. WLAN-Hotspot (`deploy/`)

- `deploy/setup_hotspot.sh`: Richtet per `nmcli` einen NetworkManager-Hotspot
  "SmartGarden" (WPA2) auf `wlan0` ein, Pi-IP `10.42.0.1`, startet automatisch
  beim Boot, setzt ufw-Regeln für 22/80/443/8000.
- `deploy/smart-garden-api.service`: systemd-Unit, die den FastAPI-Server
  über uvicorn als Dienst `smartgarden` laufen lässt.
- `deploy/README.md`: Schritt-für-Schritt-Anleitung fürs erste Deployment.
- **Ungetestet auf echter Pi-Hardware** – es stand in dieser Umgebung kein
  Pi zur Verfügung. Bitte auf dem Pi gegenprüfen, sobald er da ist.

### 2. Backend (`server/`)

Tech: FastAPI + Pydantic + SQLAlchemy (SQLite, WAL-Modus), argon2 für
API-Key-Hashing.

- `POST /api/v1/readings` – exakt nach `.claude/api-contract.md` v1.2:
  - Pydantic validiert Body-Felder inkl. Wertebereiche (z. B.
    `soil_moisture_pct` 0–100 oder `null`)
  - `X-API-Key`-Header wird gegen den gehashten Key des Geräts geprüft
    (argon2, dadurch zeitkonstant) → 401 bei falsch/fehlend/unbekanntem
    Gerät
  - Rate-Limit: max. 1 Request/2 s pro Gerät (In-Memory) → 429
  - Antwort enthält `commands` (aktuell immer leer/0/false – Admin-Endpunkt
    zum Setzen kommt in Phase 3), `config` (Default-Werte aus dem Vertrag)
    und `demo` (aktuell immer `null` – Demo-Modus-Endpunkte kommen Phase 2)
  - `received_at` wird serverseitig gesetzt (UTC), wie im Vertrag gefordert
- `GET /api/v1/devices/{id}/latest` – letzter Messwert eines Geräts, für
  das Dashboard. Noch **ohne Authentifizierung** (Login/Rollen sind
  Phase 2) – bewusst so, damit das Dashboard schon funktioniert.
- `GET /healthz` – einfacher Liveness-Check.
- `python -m app.cli create-device <id> [--label ...]` – legt ein Gerät an
  und gibt den API-Key **einmalig im Klartext** aus (danach nur Hash in
  der DB).
- Tabellen: `devices`, `readings` (alle Vertragsfelder inkl. Rohwerte,
  RSSI, Uptime, Fehler, `demo_overrides`), `device_config` (Config pro
  Gerät, aktuell nur Defaults), `pending_commands` (Ein-mal-Befehle,
  aktuell nur Struktur, noch kein Admin-Endpunkt zum Setzen).

### 3. Dashboard (`web/`)

Tech: React 19 + Vite + TypeScript + Tailwind v4.

- `Dashboard`-Komponente pollt `GET /api/v1/devices/{id}/latest` alle 5 s
  (Vite-Dev-Proxy `/api` → `localhost:8000`) und zeigt alle aktuellen
  Sensordaten: Bodenfeuchte (inkl. Rohwert), Licht (inkl. Rohwert),
  Temperatur, Luftfeuchte, Tank (mit Hinweis "Schätzung"), Pumpe,
  Gerätestatus (RSSI/Uptime/Firmware-Version), Fehler.
- Fehlercodes werden in verständlichen deutschen Text übersetzt (siehe
  Abschnitt "Neu seit dem ESP-Merge" unten) statt nur den rohen Code
  anzuzeigen.
- Farbskala Bodenfeuchte Rot→Grün wie die LED-Bar auf dem ESP, aber Farbe
  ist **nie die einzige Information** (Zahl/Text steht immer dabei) –
  WCAG-Vorgabe aus dem Plan.
- Banner für DEMO-Modus ist vorbereitet (`demo_overrides`-Feld), zeigt
  sich sobald der Server im Demo-Modus Werte überschreibt (Phase 2).
- Geräte-ID ist noch fest auf `esp32-kuebel-01` verdrahtet –
  Geräteauswahl kommt mit Login/Rollen in Phase 2.

## Getestet

Da kein Pi/ESP zur Verfügung stand, lief der komplette Test lokal gegen
`tools/fake_esp.py` (von Florians Branch, mittlerweile auf `main`):

- Gültiger Request → 200, `commands`/`config`/`demo` wie im Vertrag
- Falscher/fehlender API-Key, unbekanntes Gerät → 401
- Zu schnelle Requests (< 2 s Abstand) → 429
- `GET .../latest` liefert exakt das zurück, was zuletzt gesendet wurde
- Frontend + Backend zusammen über den Vite-Dev-Proxy durchgespielt

Dabei einen Bug gefunden und behoben: Beim allerersten Request eines neu
angelegten Geräts warf der Server einen 500er, weil die
SQLAlchemy-Objektdefaults für `pending_commands` erst beim DB-Flush
gesetzt werden, nicht beim Erzeugen des Python-Objekts. Fix: Defaults
jetzt explizit beim Anlegen gesetzt (`server/app/routers/readings.py`).

## Neu seit dem ESP-Firmware-Merge (PR #5, #3)

Mit der fertigen ESP32-Firmware und dem aktualisierten
`docs/hardware/verkabelung.md` kamen dazu:

- **Bodensensor gewechselt** auf kapazitiv (HW-390) – ändert nur die
  Hardware, nicht den API-Vertrag (`soil_moisture_pct`/`soil_moisture_raw`
  bleiben gleich). Keine Backend-/Frontend-Änderung nötig.
- **LED-Bar-Richtung bestätigt**: Rot = trocken, Grün = feucht – passt zu
  der Farbskala, die das Dashboard schon hatte.
- **Konkrete Fehlercodes aus der Firmware** (`smart_garden.ino`):
  `soil_out_of_range`, `light_read_failed`, `dht_read_failed`,
  `tank_empty`. Das Dashboard übersetzt diese jetzt in verständlichen
  Text (z. B. "Bodensensor außerhalb des Messbereichs" statt
  `soil_out_of_range`), mit dem Rohcode als Tooltip für unbekannte/neue
  Codes.
- `tools/fake_esp.py` ist jetzt auf `main` – kein manuelles Auschecken
  aus einem Feature-Branch mehr nötig zum Testen.

Der API-Vertrag selbst (`.claude/api-contract.md`) wurde **nicht**
geändert, daher war keine Vertragsanpassung nötig.

## Diagramme im Dashboard (Phase 2, Teil 1)

Unter den Kacheln gibt es jetzt den Bereich **Verlauf**:

- Zeitraum umschaltbar: 1 Stunde, 24 Stunden, 7 Tage (Aktualisierung alle 30 s).
- Kennzahlen für den Zeitraum: Wasserverbrauch (geschätzt aus Pumpenlaufzeit ×
  Durchfluss), wie oft automatisch gegossen wurde, Ø Bodenfeuchte.
- **Bodenfeuchte** mit Gießschwelle und Zielwert (gestrichelt) und orangen
  Punkten dort, wo der ESP selbst gegossen hat – man sieht den Sägezahn aus
  Austrocknen und Gießen.
- **Wassertank** (Schätzung) mit Warnschwelle.
- **Wasserverbrauch** als Balken pro 5 Minuten / Stunde / Tag.
- **Temperatur, Luftfeuchte, Licht** (Tag-Nacht-Verlauf).
- Demo-Zeiträume sind in allen Diagrammen hinterlegt und mit „DEMO“ beschriftet;
  fällt ein Sensor aus, hat die Linie eine Lücke, der Tooltip nennt den Fehler.
- Jede Grafik lässt sich als Tabelle aufklappen; Farben sind für Farbenblindheit
  und hell/dunkel geprüft.

Backend dazu: `GET /api/v1/devices/{id}/readings?from=&to=&bucket=` (Mittelwerte
pro Zeitabschnitt) und `GET /api/v1/devices/{id}/config`. Zum Ausprobieren ohne
Hardware: `python -m app.dev_seed esp32-kuebel-01 --hours 48` füllt die
**Entwicklungs**-Datenbank mit 48 h simuliertem Verlauf.

Behoben: Zeitstempel wurden ohne Zeitzone ausgeliefert, der Browser zeigte
„Letzter Kontakt“ deshalb 2 h falsch an.

## Demo-Modus für die Präsentation (Phase 2, Teil 2)

Im Dashboard gibt es die **Demo-Steuerung** (unter den Kacheln, oben rechts
ein-/ausblendbar). Sie überschreibt Messwerte **auf dem ESP** – so reagiert die
echte Hardware (Pumpe, LED-Bar, Summer), wie im API-Vertrag Abschnitt 4
vorgesehen. Die Sicherheitsgrenzen der Pumpe gelten weiter.

- **Szenarien per Klick:** Trockene Erde (12 % → ESP gießt), Hitzewelle
  (38 °C / 25 %), Tank fast leer (4 %), Sensorausfall (DHT11), Nacht (Licht 2 %).
- **Eigene Werte:** Schieberegler für Bodenfeuchte, Licht, Temperatur,
  Luftfeuchte, Tank und Checkboxen für Sensorausfälle.
- **Dauer** 1/2/5/10 min mit Countdown; „Demo beenden“ schaltet sofort zurück.
  Der ESP übernimmt Änderungen mit seiner nächsten Meldung (alle ~15 s) – die
  Steuerung zeigt an, sobald das Gerät die Demo-Werte meldet.
- **Gerät:** Jetzt gießen (5 s), Tank aufgefüllt, Gerät finden (LED blinkt),
  Piepen – jeweils genau einmal an den ESP zugestellt.
- Solange die Demo läuft, sehen **alle** oben einen violetten Banner; betroffene
  Kacheln tragen „DEMO“, im Verlauf ist der Zeitraum hinterlegt.

**Tipp für die Präsentation:** Nach „Tank fast leer“ merkt sich der ESP die
Tank-Schätzung von 4 % – danach „Tank aufgefüllt“ drücken.

**Noch offen:** Laut Plan dürfen das nur Admins. Login/Rollen gibt es noch
nicht, bis dahin kann jeder im Hotspot-WLAN die Demo auslösen. Jede Änderung
wird im Server-Terminal protokolliert.

Backend: `GET/PUT/DELETE /api/v1/devices/{id}/demo`,
`GET/POST /api/v1/devices/{id}/commands`.

## Dashboard auf dem Handy

- **Zu Hause / in der Entwicklung:** `npm run dev:mobil` statt `npm run dev`
  (im Ordner `web`). Vite zeigt dann eine Zeile `Network: http://192.168.x.y:5173` –
  diese Adresse auf dem Handy öffnen (Handy im selben WLAN wie der Mac). Beim
  ersten Mal fragt macOS evtl., ob „node“ eingehende Verbindungen annehmen darf →
  erlauben. Das Backend bleibt auf dem Mac (`127.0.0.1:8000`), Vite leitet `/api` weiter.
- **Auf dem Pi (Hackathon):** `npm run build` erzeugt `web/dist/`; FastAPI liefert das
  dann selbst unter `/` aus. Handy ins WLAN „SmartGarden“, `http://10.42.0.1:8000`.
- **Als App:** iPhone „Teilen → Zum Home-Bildschirm“, Android „App installieren“ –
  eigenes Icon (Keimling mit Wassertropfen), startet ohne Browserleiste.
- Handy-Layout: Kacheln zweispaltig, Tipp-Flächen ≥ 44 px, Abstand zu Notch und
  Home-Leiste, Demo-Steuerung zweispaltig, Diagramme per Antippen (Tooltip).
  Geprüft als iPhone 13 (hell/dunkel): kein seitliches Scrollen, keine Konsolenfehler.

**Achtung:** Mit `dev:mobil` sieht **jeder im selben WLAN** das Dashboard – inklusive
Demo-Steuerung, solange es noch keinen Login gibt. Zu Hause ok, in fremden WLANs
(Schule, Hackathon) besser den normalen `npm run dev` nutzen.

## Deployment auf den Raspberry Pi

Ein Befehl vom Laptop aus: `./deploy/deploy_to_pi.sh <benutzer>@<pi-adresse>`
(Details in `deploy/README.md`). Das Skript baut das Dashboard, kopiert alles
auf den Pi und richtet dort Server (systemd-Dienst, Port 8000, liefert API und
Dashboard), Gerät + API-Key und den WLAN-Hotspot „SmartGarden“ ein.

- Der Hotspot ist auf den ESP32 abgestimmt: nur 2,4 GHz (Kanal 6), reines WPA2,
  PMF aus – mit WPA3/5 GHz verbindet sich der ESP32 oft nicht.
- Zugangsdaten (WLAN-Passwort, API-Key) werden **auf dem Pi erzeugt** und nur dort
  gespeichert (`/opt/smart-garden/zugangsdaten.txt`, nur root). Das Skript zeigt sie
  am Ende an und legt eine fertige `firmware/smart_garden/secrets.h` auf dem Laptop ab
  (von Git ignoriert). So landet nichts davon im öffentlichen Repo.
- Wiederholbar: Datenbank, Key und Passwort bleiben erhalten.
- Getestet in einer Linux-Umgebung mit Python 3.11 (wie Raspberry Pi OS Bookworm);
  WLAN/systemd waren dort nur simuliert – der echte Hotspot muss auf dem Pi geprüft werden.

## Neues Design (einfarbig, hell/dunkel)

Das Dashboard soll von allen verstanden werden, nicht nur vom Team:

- **Oben eine Statuskarte in ganzen Sätzen**: „Deiner Pflanze geht es gut“ oder
  konkret, was zu tun ist („Wassertank: Fast leer – bald auffüllen“), dazu
  Online/Offline und wann sich der Kübel zuletzt gemeldet hat.
- **Kacheln mit Einordnung** statt nur Zahlen („Gut feucht“, „Genug Wasser“,
  „Angenehm“), Balken für Bodenfeuchte und Tank. Technische Angaben (WLAN-Signal,
  Firmware, Rohwerte) stehen unten unter „Gerät“.
- **Einfarbig**: ein Grünton als Akzent, sonst Grautöne. Zustände immer als
  Symbol + Text, nie nur über Farbe.
- **Hell / Dunkel / wie das Gerät** über den Umschalter oben rechts; die Wahl
  merkt sich der Browser.
- **Icons** aus `lucide-react` (werden mitgebaut – kein Internet nötig).
- Die **Demo-Steuerung** ist erst nach Klick auf „Demo“ sichtbar.

## Bekannte Lücken / Als Nächstes

- Deployment + Hotspot auf echtem Pi verifizieren
- Phase 2 (Rest): Warnungen/Alerts, Login+Rollen
  (argon2 ist schon eingebunden), i18n (NL/DE/EN), KI-Stub
- API-Key + WLAN-Passwort: erzeugt `deploy_to_pi.sh` – Florian bekommt die
  `secrets.h` bzw. die Werte direkt (nicht über GitHub)
