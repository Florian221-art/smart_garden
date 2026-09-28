# Pinout und Verkabelungsplan

Stand: 28.09.2026 · Board: ESP32 DevKit V1 (ESP-WROOM-32, 30 Pins, CP2102)

> **Sicherheit:** Die 12 V der Pumpe dürfen **nie** an einen ESP32-Pin oder an die Plus-Schiene des Breadboards kommen. Der 12-V-Kreis läuft nur über die Schraubklemmen des Relais. Erst alles verkabeln, dann Strom anschließen.

## 1. Pinbelegung auf einen Blick

| ESP32-Pin | Beschriftung Board | Funktion | Bauteil | Richtung |
|---|---|---|---|---|
| GPIO34 | D34 | Bodenfeuchte analog | IDUINO S | Eingang (ADC1) |
| GPIO25 | D25 | Versorgung Bodensensor (nur beim Messen an) | IDUINO + | Ausgang |
| GPIO35 | D35 | Licht analog | LDR-Modul AO | Eingang (ADC1) |
| GPIO32 | D32 | *reserviert:* Tank-Füllstand | – | Eingang (ADC1) |
| GPIO4 | D4 | Temperatur/Luftfeuchte (DHT-Protokoll) | Grove DHT11 SIG | bidirektional |
| GPIO18 | D18 | LED-Bar Daten | Grove LED Bar DI | Ausgang |
| GPIO19 | D19 | LED-Bar Takt | Grove LED Bar DCKI | Ausgang |
| GPIO26 | D26 | Summer | Piezo (+) bzw. NPN-Basis | Ausgang |
| GPIO27 | D27 | Pumpen-Relais | Grove Relay SIG | Ausgang |
| GPIO2 | D2 | Status-LED (onboard) | – | Ausgang |
| 3V3 | 3V3 | 3,3 V Versorgung | alle Module außer Summer Var. B | – |
| VIN | VIN | 5 V vom USB | nur Summer (Variante B) | – |
| GND | GND | Masse | alle | – |

Warum diese Pins:

- Analoge Sensoren nur an **ADC1** (GPIO32–39). ADC2 funktioniert nicht, solange WLAN aktiv ist.
- GPIO34/35 sind reine Eingänge – perfekt für Sensoren.
- Strapping-Pins (GPIO0, 2, 12, 15) nicht für externe Bauteile, sonst bootet der ESP evtl. nicht.
- GPIO27 ist beim Booten LOW → Pumpe läuft beim Einschalten nicht an.

## 2. Grove-Kabelfarben

DHT11, LED-Bar und Relais haben 4-polige Grove-Buchsen. Mit einem Grove-auf-Jumper-Kabel gilt:

| Farbe | Grove-Pin | Bedeutung |
|---|---|---|
| Gelb | 1 | Signal 1 (SIG bzw. DI) |
| Weiß | 2 | Signal 2 (NC bzw. DCKI) |
| Rot | 3 | VCC |
| Schwarz | 4 | GND |

## 3. Verkabelung pro Bauteil

### Stromschienen des Breadboards

- ESP32 **3V3** → rote Schiene (+) · ESP32 **GND** → blaue Schiene (−)
- Alles, was „3V3“ oder „GND“ braucht, von diesen Schienen nehmen.

### Bodenfeuchtesensor IDUINO (S / + / −)

| Sensor | → | ESP32 |
|---|---|---|
| S | → | GPIO34 |
| + | → | GPIO25 |
| − | → | GND |

Der Sensor bekommt nur Strom, während gemessen wird (GPIO25 kurz HIGH). Das verhindert, dass die Metallstreifen im feuchten Boden schnell korrodieren.

### Lichtsensor LDR-Modul (VCC / GND / DO / AO)

| Modul | → | ESP32 |
|---|---|---|
| VCC | → | 3V3 |
| GND | → | GND |
| AO | → | GPIO35 |
| DO | → | nicht anschließen |

Hinweis: Bei diesen Modulen ist der AO-Wert meist **hoch, wenn es dunkel ist**. Die Firmware rechnet das um.

### Temperatur/Luftfeuchte Grove v1.2 (DHT11)

| Grove-Kabel | → | ESP32 |
|---|---|---|
| Gelb (SIG) | → | GPIO4 |
| Weiß (NC) | → | nicht anschließen |
| Rot (VCC) | → | 3V3 |
| Schwarz (GND) | → | GND |

### Grove LED Bar v2.0

| Grove-Kabel | → | ESP32 |
|---|---|---|
| Gelb (DI) | → | GPIO18 |
| Weiß (DCKI) | → | GPIO19 |
| Rot (VCC) | → | 3V3 |
| Schwarz (GND) | → | GND |

### Grove Relay (Pumpe)

Steuerseite (Grove-Buchse):

| Grove-Kabel | → | ESP32 |
|---|---|---|
| Gelb (SIG) | → | GPIO27 |
| Weiß (NC) | → | nicht anschließen |
| Rot (VCC) | → | 3V3 (Relais hat eine 3-V-Spule, **nicht** an 5 V) |
| Schwarz (GND) | → | GND |

Lastseite (grüne Schraubklemme J1, 2 Anschlüsse = Schließer):

```
12-V-Netzteil (+) ────────── Klemme J1 (links)
                              [Relais-Kontakt]
                             Klemme J1 (rechts) ──── Pumpe (+)
12-V-Netzteil (−) ──────────────────────────────── Pumpe (−)

Freilaufdiode 1N4007 direkt an der Pumpe:
  Kathode (Ring) an Pumpe (+), Anode an Pumpe (−)
```

- Das Relais trennt den 12-V-Kreis galvanisch vom ESP32 – **keine** gemeinsame Masse nötig.
- Pumpe ohne markierte Polarität? Kurz testen; pumpt sie falsch herum, Anschlüsse tauschen (Diode mit umdrehen).
- Pumpe nie trocken laufen lassen.

### Summer LF-PB30W35B (rot = +, schwarz = −)

**Variante A – direkt (erst testen):**

| Summer | → | ESP32 |
|---|---|---|
| Rot (+) | → | GPIO26 |
| Schwarz (−) | → | GND |

Der Summer ist für 9 V ausgelegt, an 3,3 V ist er leiser. Wenn er laut genug ist, reicht Variante A.

**Variante B – lauter, mit NPN-Transistor (BC547/2N2222) + 1 kΩ:**

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
   Boden S (IDUINO) -----> | D34                 RX0 |
   LDR AO -------------->  | D35                 D21 |
   (Tank, spaeter) ----->  | D32                 D19 | -----> LED-Bar DCKI (weiss)
                           | D33                 D18 | -----> LED-Bar DI (gelb)
   Boden + (Versorg.) <--- | D25                  D5 |
   Summer <--------------- | D26                 TX2 |
   Relais SIG (gelb) <---- | D27                 RX2 |
                           | D14                  D4 | <----> DHT11 SIG (gelb)
                           | D12                  D2 |  (Onboard-LED)
                           | D13                 D15 |
                           | GND                 GND | -----> GND-Schiene
   (Summer Var. B) 5 V --- | VIN                 3V3 | -----> 3V3-Schiene
                           +---------[USB]-----------+

   3V3-Schiene: LDR VCC, DHT11 VCC, LED-Bar VCC, Relais VCC
   GND-Schiene: LDR GND, DHT11 GND, LED-Bar GND, Relais GND, Bodensensor −, Summer −
```

## 5. Besorgliste

- [ ] Grove-auf-Jumper-Kabel (Buchse) ×3 für DHT11, LED-Bar, Relais
- [ ] Jumper-Kabel männlich-männlich / männlich-weiblich
- [ ] 12-V-Netzteil ≥ 1,5 A mit Hohlstecker-Adapter/Schraubklemme
- [ ] Freilaufdiode 1N4007
- [ ] Silikonschlauch passend zur Pumpe, Wasserbehälter
- [ ] optional: NPN-Transistor + 1 kΩ (Summer), Füllstandssensor/Schwimmerschalter
- [ ] Micro-USB-**Datenkabel** für den ESP32 (kein reines Ladekabel)

## 6. Inbetriebnahme in dieser Reihenfolge

1. Nur ESP32 per USB → Testprogramm flashen, serielle Ausgabe prüfen
2. DHT11 anschließen → Temperatur/Luftfeuchte prüfen
3. LDR → Wert mit Hand abdecken, muss sich ändern
4. Bodensensor → trocken / in Wasserglas messen (Kalibrierwerte notieren!)
5. LED-Bar, Summer
6. Relais **ohne** Pumpe → Klicken hören, LED am Relais leuchtet
7. Zuletzt 12-V-Kreis mit Pumpe (Schlauch im Wasser!)
