# Smart Garden ESP32 firmware

C++ on the Arduino framework (ESP32 core 3.x), uploaded with the Arduino IDE. Current version: **0.3.8**.

The firmware measures soil moisture, light, air temperature and humidity. It shows the soil moisture on the LED bar, waters the plant on its own through a relay and pump, and sends all readings to the server on the Raspberry Pi. Without Wi-Fi or server, everything except sending keeps working.

> **Note:** the firmware's serial monitor output and serial commands are in German. They are quoted verbatim below, with the English meaning next to them.

| Further docs | Contents |
|---|---|
| [`docs/hardware/verkabelung.md`](../../docs/hardware/verkabelung.md) | Pinout, wiring, commissioning |
| [`docs/hardware/sicherheit-esp.md`](../../docs/hardware/sicherheit-esp.md) | Threat model and protective measures for the ESP32 part |
| [`.claude/api-contract.md`](../../.claude/api-contract.md) | Interface to the server (JSON fields, demo mode) |
| [`tools/`](../../tools/README.md) | `fake_esp.py`: simulates this ESP32, for testing the server without hardware |

---

## 1. One-time setup: Arduino IDE

1. Install **Arduino IDE 2.x**.
2. Under *File → Preferences → Additional boards manager URLs* add:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
3. Under *Tools → Board → Boards Manager* install **esp32 by Espressif Systems**, version **3.x** (tested with 3.3.12).
4. Under *Tools → Manage Libraries* install:
   - **DHT sensor library** (Adafruit). When asked about dependencies, choose "Install all"; this also installs Adafruit Unified Sensor.
   - **ArduinoJson** by Benoit Blanchon, version **7.x**
   - The Grove LED Bar needs **no** library; the driver is built into `ledbar.cpp`.
5. If Windows shows no COM port: install the driver for the **CP2102** USB chip (Silicon Labs).

## 2. Configuration

The settings are at the very top of **`smart_garden.ino`**, in the block **"EINSTELLUNGEN – HIER ANPASSEN"** (settings – adjust here):

| Variable | Meaning |
|---|---|
| `CFG_WIFI_SSID` | Wi-Fi name, `SmartGarden` (the Pi's hotspot) |
| `CFG_WIFI_PASSWORD` | Wi-Fi password (from Nico) |
| `CFG_SERVER_URL` | `http://10.42.0.1:8000`, later `https://10.42.0.1` |
| `CFG_API_KEY` | generated on the server when the device is created |
| `CFG_DEVICE_ID` | `esp32-kuebel-01`, must match the device on the server |
| `CFG_LEDBAR_MODE` | `2` = dryness bar starting from green (default), `0` = pointer, `1` = fill bar starting from red (section 6) |
| `CFG_LEDBAR_REVERSE` | `true` if the display appears mirrored |
| `CFG_LEDBAR_SWAP_PINS` | `true` if the LED bar does not react at all (clock/data swapped) |
| `CFG_BUZZER_ENABLED` | `false` mutes the buzzer completely |

### Safer: credentials in `secrets.h`

The repository is **public**. A real password at the top of the sketch can be committed by accident. To avoid this:

1. Copy `secrets.h.example` in the same folder and rename the copy to **`secrets.h`**.
2. Enter the password and API key there.
3. Reopen the Arduino IDE and upload.

`secrets.h` is listed in `.gitignore` and is therefore never pushed. Its values take precedence over the sketch. At startup the monitor then shows `[CFG] Zugangsdaten aus secrets.h übernommen` (credentials loaded from secrets.h).

The Pi deploy script (`deploy/deploy_to_pi.sh`) creates a ready-made `secrets.h` in this folder, see [`deploy/README.md`](../../deploy/README.md).

If you do edit the sketch directly: run `git stash` before every `git pull`, then `git stash pop`. Before committing, check with `git diff` that no password is included.

If the placeholders are still in place, the firmware prints `WARNUNG: ... Platzhalter` (warning: ... placeholder) at startup. If the Wi-Fi password is the placeholder, Wi-Fi stays off entirely (`[WLAN] AUS: ...` – Wi-Fi off). Sensors, LED bar, pump and demo commands still work; only sending does not.

If the connection fails, the serial monitor states the reason once in plain text:

| Message | Meaning |
|---|---|
| `"SmartGarden" nicht gefunden` (not found) | Pi hotspot is off or too far away |
| `gefunden, aber Anmeldung abgelehnt` (found, but login rejected) | Wrong Wi-Fi password |
| `Verbindung abgerissen` (connection dropped) | Signal too weak |

The ESP32 then retries every 15 s, but only once the previous attempt has finished.

## 3. Uploading

1. Open `firmware/smart_garden/smart_garden.ino`. All other files appear as tabs.
2. Under *Tools → Board* select **ESP32 Dev Module**, and under *Port* the ESP32's COM port.
3. Click **Upload**.
4. Open *Tools → Serial Monitor*, set **115200 baud** and line ending **"New Line"**.

At startup the monitor shows the firmware version (currently `Firmware 0.3.9`). It must match `FIRMWARE-VERSION` at the very top of `smart_garden.ino` (and `FW_VERSION` in `garden_config.h`). The LED bar fills once from green through orange to red, and the buzzer beeps briefly. If red lights up first, set `CFG_LEDBAR_REVERSE = true` at the top (to try it without re-uploading: `ledflip`).

**If the upload gets stuck** (`Connecting…` hangs or `Wrong boot mode detected`):

- Hold **BOOT** until the upload starts; if necessary, briefly press **EN** while doing so.
- Disconnect the 3V3 rail and the 12 V power supply during the upload.
- Set *Tools → Upload Speed* to **115200**.

## 4. Calibration (once, about 2 minutes)

| Step | Command |
|---|---|
| Hold the soil sensor dry in the air | `cal soil dry` |
| Put the soil sensor into a glass of water up to the white line | `cal soil wet` |
| Cover the light sensor with a finger | `cal light dark` |
| Shine a phone flashlight directly onto the light sensor | `cal light bright` |

The values stay stored on the ESP32, even after a restart or a new upload. Check them with `status`.

For the soil sensor, the "dry" raw value must be at least **300** above "wet", otherwise nothing is saved (typical: dry ~3200, wet ~1100). If the calibration is invalid, the sensor reports an error and no watering takes place. This prevents a bad calibration from being read as "0 % moisture" and triggering continuous watering.

**Measuring the pump flow rate:** hold the hose in a measuring cup, enter `pump 10` and divide the amount by 10 (via the serial console up to 15 s are allowed, regardless of the burst length). The result is the flow rate in ml/s, which is entered in the dashboard under "Durchfluss" (flow rate). The tank estimate is only correct with this value.

## 5. Serial monitor commands

| Command | Effect |
|---|---|
| `help` | Overview |
| `status` | Current values, settings, calibration, demo status (without password/key) |
| `demo soil 12` | Fake a soil moisture of 12 %: LED bar full, the pump starts once for one burst (0.5 s). Lasts 120 s; optionally give the seconds as a 3rd value, at most 600 s |
| `demo temp 38` / `demo hum 25` / `demo light 2` | Fake temperature / humidity / light |
| `demo tank 4` | Fake an almost empty tank: buzzer, no watering. The real tank estimate stays unchanged |
| `demo error dht` | Simulate a DHT11 sensor failure (also `soil`, `light`) |
| `demo off` | Back to real values |
| `pump 0.5` | Run the pump for 0.5 s (use a dot, not a comma; up to 15 s; pauses/tank/daily limit apply) |
| `stop` | Pump off immediately |
| `refill` | Mark the tank as refilled (or hold the BOOT button for 3 s). Also lifts the watering lock |
| `cal soil dry\|wet`, `cal light dark\|bright` | Calibration (section 4) |
| `ledtest` / `ledflip` | Test the LED bar / reverse its direction (until restart) |
| `ledseg 3` | Show exactly 3 segments (starting at segment 1 = red) for 10 s |
| `ledswap` | Swap clock and data pin (until restart) if the bar does not react |
| `leddiag` | LED bar: 8 transmission variants one after another (4 s each), for troubleshooting |
| `beep` | Test the buzzer (beeps even when muted) |
| `mute` | Buzzer on/off (until restart) |
| `send` | Send to the server immediately |

Unknown input is answered with `Unbekannter Befehl – "help" eingeben` (unknown command – enter "help").

Demo mode can also be triggered with a button in the dashboard. The firmware reports demo values to the server in the `demo_overrides` field, so they are marked in the dashboard and not used for AI training.

## 6. Behaviour

### Measuring and display

- Every 2 s the firmware measures soil moisture (GPIO34), light (GPIO35) and temperature/humidity (DHT11 on GPIO4, at most every 5 s). For each analog value it takes the median of 5 samples.
- **LED bar** in the default mode `2`: the drier the soil, the more LEDs light up.

  | Soil moisture | LEDs |
  |---|---|
  | 100 % | 1 green |
  | 90 % | 2 green |
  | 80 % | 3 green |
  | 70 % | 4 green |
  | 60 % | 5 green |
  | 50 % | 6 green |
  | 40 % | 7 green |
  | 30 % | 8 green |
  | 11–20 % | 8 green + orange |
  | 10 % and less | all 10 (8 green + orange + red) |

  Special displays: on a sensor error, segments 1 and 10 light up alternately. When the tank is empty, segment 1 blinks rapidly. The dashboard command "identify" shows a running light for 5 s.
- **Onboard LED**: on = Wi-Fi OK, slow blinking = connecting, fast blinking = sending failed.

### Watering

- After power-on, automatic watering first waits for `pump_cooldown_s` (default 5 min). This protects against a pump burst on every boot if the ESP32 keeps restarting. Demo and manual commands are possible after 10 s.
- If moisture falls below `moisture_min_pct` (default 30 %), a watering session starts. The pump runs in bursts of `max_pump_s_per_run` each (default **0.5 s**, also used for demo and the dashboard button "Jetzt gießen" (water now)), with a pause of `pump_cooldown_s` (300 s) in between so the water can soak in. This continues until `moisture_target_pct` (55 %) is reached.
- The server can change `max_pump_s_per_run` via `config`, but the firmware clamps it to **0.3–1 s** (a small splash). Demo watering (`demo soil 12`) always runs exactly **0.5 s**. Only the serial `pump` command can run longer (up to 15 s, e.g. for measuring the flow rate).
- **Tank estimate** without a sensor: remaining amount = capacity − run time × flow rate. It is stored in flash and survives restarts. After refilling, enter `refill`, hold the BOOT button for 3 s, or press "Tank aufgefüllt" (tank refilled) in the dashboard.

### Safety

- **Safety limits**, compiled in and not changeable by the server:
  - at most 15 s per run
  - daily limit of at most 300 s
  - at least 10 s pause before every start, including dashboard and demo commands
  - no pumping at a tank level ≤ 5 %
- **Plausibility check**: if moisture rises by less than 3 percentage points after at least 3 runs with a total of at least 5 s pump time (and at least 3 min), the firmware locks automatic watering and raises an alarm. Possible causes: sensor not in the soil, hose pointing elsewhere, pump sucking air. The lock is lifted by `refill`, the BOOT button, or when moisture rises again.
- **Watchdog**: if `loop()` hangs for more than 20 s, the ESP32 restarts. The relay is switched off first. At startup the monitor shows the reason for the last restart (`[BOOT] ...`).
- No data is sent while the pump is running, so a slow server cannot delay switching it off.

### Alarm and server

- **Buzzer**: beeps only when a problem occurs **for the first time**, then at most every 10 min. Problems are an empty tank, a sensor error (only after 60 s) and ineffective watering. The reason is printed in the monitor as `[ALARM] …`.
- **Server**: the firmware sends to `POST /api/v1/readings` every `interval_s` seconds (firmware default 10; the server's `config` value takes precedence once connected) and applies `config`, `commands` and `demo` from the response.

## 7. Code structure

| File | Responsibility |
|---|---|
| `smart_garden.ino` | Settings (top), `setup()`/`loop()`, measurement cycle, watering logic with plausibility check, building and sending JSON, processing the server response, serial console, watchdog |
| `garden_config.h/.cpp` | Pins, **hard safety limits**, settings and calibration in flash (NVS), clamping of all server values, reading `secrets.h` |
| `sensors.h/.cpp` | Soil moisture (capacitive), LDR, DHT11; median filter, plausibility check, applying the calibration |
| `pump.h/.cpp` | Non-blocking relay control; every check before switching on (tank, pause, run time, daily limit) |
| `tank.h/.cpp` | Tank estimate in flash; demo fill level as display only (can only make the tank emptier) |
| `demo.h/.cpp` | Demo mode: overrides with value ranges, forced errors, expiry timer |
| `display.h/.cpp` | LED bar display and test animation, buzzer with alarm logic, status LED; all non-blocking |
| `ledbar.h/.cpp` | Custom driver for the MY9221 chip of the Grove LED Bar |
| `garden_net.h/.cpp` | Wi-Fi with reconnect, HTTP(S) POST, parsing the JSON response, optional certificate validation |
| `secrets.h.example` | Template for the optional `secrets.h` |

**Notes for making changes:**

- The file names `garden_net.*` and `garden_config.*` are deliberate. `Network.h` and `config.h` collide with files in the ESP32 core, and Windows does not distinguish upper and lower case.
- The forward declarations at the top of the `.ino` must stay: the Arduino IDE's automatic prototype generation is unreliable. Add every new `static` function in the `.ino` there.
- Do not add anything blocking to `loop()`, i.e. no `delay()` longer than a few ms. The pump is switched off via `millis()`.
- New fields in `Settings` or `Calibration` invalidate the stored state; the ESP32 then starts with default values. Recalibrate afterwards.
- When changing the version, update both `FW_VERSION` in `garden_config.h` and `FIRMWARE-VERSION` at the top of `smart_garden.ino`.
- Compile test without hardware: `arduino-cli compile --fqbn espressif:esp32:esp32 --warnings all firmware/smart_garden`. The firmware must compile without warnings.

## 8. Known limitations

- **No level sensor**: the tank estimate is only correct if the flow rate was measured and `refill` was triggered after every refill.
- **No clock**: the daily limit applies to each 24 h of uptime. After a restart the window starts over.
- **HTTP security**: over HTTP the API key can be read by others on the Wi-Fi. HTTPS with `SERVER_CA_CERT` is prepared and can be used as soon as Caddy runs on the Pi.
- **Server commands**: the ESP32 cannot check *who* triggered a command in the dashboard. The server has to secure this (admin login). The hard limits above cap the possible damage.
