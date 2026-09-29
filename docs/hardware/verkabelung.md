# Pinout und Verkabelungsplan

Stand: 29.09.2026 (v2: kapazitiver Bodenfeuchtesensor, Firmware v0.3.0) · Board: ESP32 DevKit V1 (ESP-WROOM-32, 30 Pins, CP2102)

> **Sicherheit:** Die 12 V der Pumpe dürfen **nie** an einen ESP32-Pin oder an die Plus-Schiene des Breadboards kommen. Der 12-V-Kreis läuft nur über die Schraubklemme des Relais. Erst alles verkabeln, dann Strom anschließen.

## 1. Pinbelegung auf einen Blick

| ESP32-Pin | Beschriftung Board | Bauteil | Anschluss am Bauteil |
|---|---|---|---|
| GPIO34 | D34 | Bodenfeuchtesensor (kapazitiv v2.0, HW-390) | AOUT (gelb) |
| GPIO35 | D35 | Lichtsensor LDR-Modul | AO |
| GPIO4 | D4 | Temp./Luftfeuchte Grove v1.2 (DHT11) | SIG (gelb) |
| GPIO18 | D18 | Grove LED Bar v2.0 | DI / Daten (gelb) |
| GPIO19 | D19 | Grove LED Bar v2.0 | DCKI / Takt (weiß) |
| GPIO26 | D26 | Summer | rot (+) |
| GPIO27 | D27 | Grove Relay (Pumpe) | SIG (gelb) |
| GPIO0 | – | BOOT-Taste (onboard) | 3 s halten = Tank aufgefüllt |
| GPIO2 | – | Onboard-LED | WLAN-Status |
| 3V3 | 3V3 | **alle** Module | VCC (rot) |
| GND | GND | **alle** Module | GND (schwarz) |

Frei/reserviert: GPIO25, GPIO32 (ADC1) für spätere Erweiterungen.

Warum diese Pins:

- Analoge Sensoren nur an **ADC1** (GPIO32–39). ADC2 funktioniert nicht, solange WLAN aktiv ist.
- Strapping-Pins (GPIO0, 2, 12, 15) nicht für externe Bauteile, sonst bootet der ESP evtl. nicht.
- GPIO27 ist beim Booten LOW → die Pumpe läuft beim Einschalten nicht an.
- **Alles läuft mit 3,3 V** – kein Bauteil an 5 V/VIN anschließen (Ausnahme: Summer Variante B).

## 2. Kabelfarben

Grove-Stecker (DHT11, LED-Bar, Relais – 4-polig) und Bodensensor (3-polig):

| Farbe | Bedeutung |
|---|---|
| Gelb | Signal (SIG / DI / AOUT) |
| Weiß | 2. Signal (nur LED-Bar) |
| Rot | VCC → **3V3** |
| Schwarz | GND |

## 3. Verkabelung pro Bauteil

### Stromschienen des Breadboards

- ESP32 **3V3** → rote Schiene (+)
- ESP32 **GND** → blaue Schiene (−)
- Achtung: Bei langen Breadboards sind die Schienen oft **in der Mitte unterbrochen** (Lücke in der roten/blauen Linie) → ggf. mit kurzem Kabel überbrücken.

### Bodenfeuchtesensor „Capacitive Soil Moisture Sensor v2.0“ (GND / VCC / AOUT)

| Sensor | Kabel | → | ESP32 |
|---|---|---|---|
| AOUT | gelb | → | GPIO34 |
| VCC | rot | → | 3V3-Schiene |
| GND | schwarz | → | GND-Schiene |

- Nur bis zur **weißen Linie** in die Erde stecken – die Elektronik oben darf nicht nass werden.
- Kapazitiv = keine offenen Metallkontakte → korrodiert nicht, darf dauerhaft Strom haben.
- Trocken = hoher Rohwert (~3 200), nass = niedriger Rohwert (~1 100). Einmal kalibrieren (Firmware-README).

### Lichtsensor LDR-Modul (VCC / GND / DO / AO)

| Modul | → | ESP32 |
|---|---|---|
| VCC | → | 3V3-Schiene |
| GND | → | GND-Schiene |
| AO | → | GPIO35 |
| DO | → | **nicht anschließen** |

### Temperatur/Luftfeuchte Grove v1.2 (DHT11)

| Grove-Kabel | → | ESP32 |
|---|---|---|
| Gelb (SIG) | → | GPIO4 |
| Weiß (NC) | → | nicht anschließen |
| Rot (VCC) | → | 3V3-Schiene |
| Schwarz (GND) | → | GND-Schiene |

Pull-up-Widerstand ist auf dem Grove-Board schon drauf.

### Grove LED Bar v2.0

| Grove-Kabel | → | ESP32 |
|---|---|---|
| Gelb | → | GPIO18 (bei der LED-Bar = Daten DI) |
| Weiß | → | GPIO19 (bei der LED-Bar = Takt DCKI) |
| Rot (VCC) | → | 3V3-Schiene |
| Schwarz (GND) | → | GND-Schiene |

Die Bar hat Segment 1 = rot, 2 = orange, 3–10 = grün. Standardanzeige: je trockener, desto mehr LEDs (siehe Firmware-README). Laut Aufdruck auf der Platine (DI, DCKI, VCC, GND) ist gelb = DI (Daten) und weiß = DCKI (Takt), am 29.09. am Board bestätigt. Sind die beiden vertauscht, leuchten alle LEDs wild durcheinander und flackern.

### Grove Relay (Pumpe)

Steuerseite (Grove-Buchse):

| Grove-Kabel | → | ESP32 |
|---|---|---|
| Gelb (SIG) | → | GPIO27 |
| Weiß (NC) | → | nicht anschließen |
| Rot (VCC) | → | 3V3-Schiene (3-V-Relais, **nicht** an 5 V) |
| Schwarz (GND) | → | GND-Schiene |

Lastseite (grüne Schraubklemme J1 = Schließer):

```
12-V-Netzteil (+) ────────── Klemme J1 (links)
                              [Relais-Kontakt]
                             Klemme J1 (rechts) ──── Pumpe (+)
12-V-Netzteil (−) ────────────────────────────────── Pumpe (−)

optional Freilaufdiode 1N4007 direkt an der Pumpe:
  Kathode (Ring) an Pumpe (+), Anode an Pumpe (−)
```

- Das Relais trennt den 12-V-Kreis vom ESP32 – **keine** gemeinsame Masse nötig.
- Pumpt sie falsch herum → Anschlüsse an der Pumpe tauschen (Diode mitdrehen).
- Pumpe nie trocken laufen lassen, Ansaugschlauch muss im Wasser liegen.

### Summer LF-PB30W35B (rot = +, schwarz = −)

**Variante A – direkt (zuerst probieren, keine Zusatzteile):**

| Summer | → | ESP32 |
|---|---|---|
| Rot (+) | → | GPIO26 |
| Schwarz (−) | → | GND-Schiene |

Der Summer ist für 9 V gebaut und an 3,3 V leiser. Test: seriell `beep`.

**Variante B – lauter (nur falls A zu leise), NPN-Transistor BC547/2N2222 + 1 kΩ:**

```
GPIO26 ── 1 kΩ ── Basis (NPN)
                  Emitter ── GND
                  Kollektor ── Summer schwarz (−)
VIN (5 V) ── Summer rot (+)
```

## 4. Gesamtübersicht (Draufsicht, USB unten)

```
                           +-------------------------+
                           | EN                  D23 |
                           | VP                  D22 |
                           | VN                  TX0 |
   Boden AOUT (gelb) ----> | D34                 RX0 |
   LDR AO -------------->  | D35                 D21 |
   (frei)                  | D32                 D19 | -----> LED-Bar Takt DCKI (weiss)
                           | D33                 D18 | -----> LED-Bar Daten DI (gelb)
   (frei)                  | D25                  D5 |
   Summer + <------------- | D26                 TX2 |
   Relais SIG (gelb) <---- | D27                 RX2 |
                           | D14                  D4 | <----> DHT11 SIG (gelb)
                           | D12                  D2 |  (Onboard-LED)
                           | D13                 D15 |
                           | GND                 GND | -----> GND-Schiene (blau)
   (Summer Var. B) 5 V --- | VIN                 3V3 | -----> 3V3-Schiene (rot)
                           +---------[USB]-----------+

   3V3-Schiene: Boden VCC, LDR VCC, DHT11 VCC, LED-Bar VCC, Relais VCC
   GND-Schiene: Boden GND, LDR GND, DHT11 GND, LED-Bar GND, Relais GND, Summer −
```

## 5. Was ihr noch braucht

**Pflicht:**

- [ ] Jumper-Kabel (Breadboard-Kabel) männlich-männlich, ca. 15 Stück
- [ ] Adapter für die Grove-/Sensorkabel aufs Breadboard – entweder Grove-auf-Jumper-Kabel (habt ihr) oder die Stecker mit männlichen Jumper-Kabeln in die Buchse stecken
- [ ] 12-V-Netzteil mit Anschluss für die zwei Kabel (Hohlstecker-auf-Schraubklemme-Adapter oder abisolierte Kabel)
- [ ] Micro-USB-**Datenkabel** (✓ vorhanden)

**Widerstände: keine nötig.** DHT11-Pull-up, LDR-Spannungsteiler und Relais-Transistor sind auf den Modulen schon verbaut.

**Optional:**

- [ ] Freilaufdiode 1N4007 an der Pumpe (Schutz vor Störimpulsen)
- [ ] NPN-Transistor BC547/2N2222 + 1-kΩ-Widerstand, falls der Summer zu leise ist
- [ ] Isolierband / Heißkleber, um die Sensor-Elektronik vor Spritzwasser zu schützen

## 6. Inbetriebnahme in dieser Reihenfolge

1. Nur ESP32 per USB → Firmware hochladen, seriellen Monitor öffnen (115200 Baud)
2. Stromschienen verbinden (3V3, GND)
3. DHT11 anschließen → `status`: Temperatur/Luftfeuchte plausibel?
4. LDR → Sensor abdecken, Wert muss sich ändern
5. Bodensensor → an der Luft vs. im Wasserglas messen, dann `cal soil dry` / `cal soil wet`
6. LED-Bar → `ledtest` (füllt sich von Grün nach Rot; beginnt sie mit Rot → `ledflip`)
7. Summer → `beep`
8. Relais **ohne** Pumpe → `pump 2`: Klicken + LED am Relais
9. Zuletzt 12-V-Kreis mit Pumpe (Schlauch im Wasser!) → `pump 3`
