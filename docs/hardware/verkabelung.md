# Wiring and Pinout

Last updated: 29 Sep 2026 (hardware v2: capacitive soil moisture sensor; pin assignment unchanged since firmware v0.3.0, current firmware v0.3.8) · Board: ESP32 DevKit V1 (ESP-WROOM-32, 30 pins, CP2102 USB-serial chip)

The pin numbers in this document match the constants in `firmware/smart_garden/garden_config.h` (section 2, "Pins"). If either is changed, the other must be updated as well.

> **Safety:** The pump's 12 V supply must **never** be connected to an ESP32 pin or to the breadboard's positive rail. The 12 V circuit runs exclusively through the relay's screw terminal. Complete all wiring first, then connect power.

## 1. Pin assignment at a glance

| ESP32 pin | Board label | Component | Connection on component |
|---|---|---|---|
| GPIO34 | D34 | Soil moisture sensor (capacitive v2.0, HW-390) | AOUT (yellow) |
| GPIO35 | D35 | Light sensor (LDR module) | AO |
| GPIO4 | D4 | Grove Temperature & Humidity Sensor v1.2 (DHT11) | SIG (yellow) |
| GPIO18 | D18 | Grove LED Bar v2.0 | DI / data (yellow) |
| GPIO19 | D19 | Grove LED Bar v2.0 | DCKI / clock (white) |
| GPIO26 | D26 | Buzzer | red (+) |
| GPIO27 | D27 | Grove Relay (pump) | SIG (yellow) |
| GPIO0 | – | BOOT button (on board) | hold for 3 s = tank refilled |
| GPIO2 | – | On-board LED | Wi-Fi status |
| 3V3 | 3V3 | **all** modules | VCC (red) |
| GND | GND | **all** modules | GND (black) |

Free/reserved: GPIO25 and GPIO32 (ADC1) for future extensions.

Why these pins:

- Analog sensors are connected to **ADC1** only (GPIO32–39). ADC2 cannot be used while Wi-Fi is active.
- Strapping pins (GPIO0, 2, 12, 15) are not used for external components; otherwise the ESP32 may fail to boot.
- GPIO27 is LOW after reset, so the pump does not start when the board is powered on. In addition, the firmware sets the relay pin LOW before configuring it as an output.
- **Everything runs on 3.3 V**: do not connect any component to 5 V/VIN (exception: buzzer option B).

## 2. Cable colours

Grove connectors (DHT11, LED bar, relay – 4-pin) and soil sensor (3-pin):

| Colour | Meaning |
|---|---|
| Yellow | Signal (SIG / DI / AOUT) |
| White | Second signal (LED bar only) |
| Red | VCC → **3V3** |
| Black | GND |

## 3. Wiring per component

### Breadboard power rails

- ESP32 **3V3** → red rail (+)
- ESP32 **GND** → blue rail (−)
- Caution: on long breadboards the rails are often **split in the middle** (visible as a gap in the red/blue line). Bridge the gap with a short jumper wire if necessary.

### Soil moisture sensor "Capacitive Soil Moisture Sensor v2.0" (GND / VCC / AOUT)

| Sensor | Wire | → | ESP32 |
|---|---|---|---|
| AOUT | yellow | → | GPIO34 |
| VCC | red | → | 3V3 rail |
| GND | black | → | GND rail |

- Insert the sensor into the soil only up to the **white line**; the electronics at the top must not get wet.
- Capacitive means there are no exposed metal contacts, so the sensor does not corrode and may be powered continuously.
- Dry = high raw value (approx. 3,200), wet = low raw value (approx. 1,100). Calibrate once (see the firmware README); until then the firmware uses default values of 2,900 (dry) and 1,300 (wet).

### Light sensor LDR module (VCC / GND / DO / AO)

| Module | → | ESP32 |
|---|---|---|
| VCC | → | 3V3 rail |
| GND | → | GND rail |
| AO | → | GPIO35 |
| DO | → | **do not connect** |

### Grove Temperature & Humidity Sensor v1.2 (DHT11)

| Grove wire | → | ESP32 |
|---|---|---|
| Yellow (SIG) | → | GPIO4 |
| White (NC) | → | do not connect |
| Red (VCC) | → | 3V3 rail |
| Black (GND) | → | GND rail |

The pull-up resistor is already fitted on the Grove board.

### Grove LED Bar v2.0

| Grove wire | → | ESP32 |
|---|---|---|
| Yellow | → | GPIO18 (on the LED bar = data, DI) |
| White | → | GPIO19 (on the LED bar = clock, DCKI) |
| Red (VCC) | → | 3V3 rail |
| Black (GND) | → | GND rail |

On the bar, segment 1 is red, segment 2 is orange and segments 3–10 are green. Default display: the drier the soil, the more LEDs are lit (see the firmware README). According to the labels printed on the board (DI, DCKI, VCC, GND), yellow = DI (data) and white = DCKI (clock); this was confirmed on the actual board on 29 Sep. If the two are swapped, the LEDs light up randomly and flicker.

### Grove Relay (pump)

Control side (Grove socket):

| Grove wire | → | ESP32 |
|---|---|---|
| Yellow (SIG) | → | GPIO27 |
| White (NC) | → | do not connect |
| Red (VCC) | → | 3V3 rail (3 V relay, **not** 5 V) |
| Black (GND) | → | GND rail |

Load side (green screw terminal J1 = normally open contact):

```
12 V power supply (+) ─────── Terminal J1 (left)
                              [relay contact]
                             Terminal J1 (right) ──── Pump (+)
12 V power supply (−) ─────────────────────────────── Pump (−)

optional flyback diode 1N4007 directly at the pump:
  cathode (ring) to pump (+), anode to pump (−)
```

- The relay isolates the 12 V circuit from the ESP32, so **no** common ground is needed.
- If the pump pumps in the wrong direction, swap the wires at the pump (and turn the diode around as well).
- Never let the pump run dry; the intake hose must be submerged in water.

### Buzzer LF-PB30W35B (red = +, black = −)

**Option A – direct connection (try this first, no extra parts):**

| Buzzer | → | ESP32 |
|---|---|---|
| Red (+) | → | GPIO26 |
| Black (−) | → | GND rail |

The buzzer is designed for 9 V and is quieter at 3.3 V. Test: serial command `beep` (two short beeps).

**Option B – louder (only if A is too quiet), NPN transistor BC547/2N2222 + 1 kΩ resistor:**

```
GPIO26 ── 1 kΩ ── base (NPN)
                  emitter ── GND
                  collector ── buzzer black (−)
VIN (5 V) ── buzzer red (+)
```

## 4. Overall layout (top view, USB at the bottom)

```
                           +-------------------------+
                           | EN                  D23 |
                           | VP                  D22 |
                           | VN                  TX0 |
   Soil AOUT (yellow) ---> | D34                 RX0 |
   LDR AO -------------->  | D35                 D21 |
   (free)                  | D32                 D19 | -----> LED bar clock DCKI (white)
                           | D33                 D18 | -----> LED bar data DI (yellow)
   (free)                  | D25                  D5 |
   Buzzer + <------------- | D26                 TX2 |
   Relay SIG (yellow) <--- | D27                 RX2 |
                           | D14                  D4 | <----> DHT11 SIG (yellow)
                           | D12                  D2 |  (on-board LED)
                           | D13                 D15 |
                           | GND                 GND | -----> GND rail (blue)
   (buzzer opt. B) 5 V --- | VIN                 3V3 | -----> 3V3 rail (red)
                           +---------[USB]-----------+

   3V3 rail: soil VCC, LDR VCC, DHT11 VCC, LED bar VCC, relay VCC
   GND rail: soil GND, LDR GND, DHT11 GND, LED bar GND, relay GND, buzzer −
```

## 5. What you still need

**Required:**

- [ ] Male-to-male jumper wires (breadboard wires), about 15
- [ ] Adapters to connect the Grove/sensor cables to the breadboard – either Grove-to-jumper cables (available) or male jumper wires plugged into the connector sockets
- [ ] 12 V power supply with a way to connect the two wires (barrel jack to screw terminal adapter, or stripped wires)
- [ ] Micro-USB **data** cable (available)

**Resistors: none needed.** The DHT11 pull-up, the LDR voltage divider and the relay driver transistor are already fitted on the modules.

**Optional:**

- [ ] 1N4007 flyback diode at the pump (protects against voltage spikes)
- [ ] NPN transistor BC547/2N2222 + 1 kΩ resistor, if the buzzer is too quiet
- [ ] Insulating tape / hot glue to protect the sensor electronics from splash water

## 6. Commissioning in this order

Serial commands are entered in the serial monitor (115200 baud, line ending "Newline"); `help` lists all commands.

1. ESP32 on USB only → upload the firmware, open the serial monitor (115200 baud).
2. Connect the power rails (3V3, GND).
3. Connect the DHT11 → `status`: are temperature and humidity plausible?
4. Connect the LDR → cover the sensor; the value must change.
5. Connect the soil sensor → measure in air and in a glass of water, then run `cal soil dry` / `cal soil wet`.
6. Connect the LED bar → `ledtest` (fills from green to red; if it starts with red, use `ledflip` to reverse the direction).
7. Connect the buzzer → `beep`.
8. Relay **without** the pump → `pump 2` (runs for 2 s): you should hear a click and see the LED on the relay light up.
9. Finally, the 12 V circuit with the pump (hose in the water!) → `pump 3`. Note that the firmware enforces a minimum pause of 10 s between two pump runs, so wait briefly after step 8.
