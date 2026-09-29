# ESP32 Security Analysis

Last updated: 29 Sep 2026 · Firmware v0.3.8 (the review log in section 6 covers the changes from v0.2.4 to v0.3.0) · Counterpart for the server/dashboard side: `docs/server/` (Nico)

This document is the ESP32 part of the threat modelling required by the hackathon task ("threat modelling (OWASP)"). It describes what is being protected, who could attack it, and which countermeasures are implemented in the ESP32 firmware. It also lists the risks that remain.

All hard limits mentioned below are compile-time constants in `firmware/smart_garden/garden_config.h` (section 3, "Hard safety limits"). The server cannot change them.

## 1. What are we protecting?

| Asset | Why it matters |
|---|---|
| **Pump, water, plant** | An actuator with real physical effect. If the pump is misused, the result can be dry running (damaged pump), flooding, wasted water or a dried-out plant. |
| **Device API key** | Anyone who has it can submit fake measurements in the planter's name. |
| **Hotspot Wi-Fi password** | Anyone who knows it is on the same network as the Raspberry Pi and the ESP32. |
| **Measurement data** | No personal data (no camera, no microphone, no user identifiers), but it is the basis for the dashboard and the AI features. |

## 2. System boundaries and data flows

```
 [Sensors] --analog/1-Wire--> [ESP32] --Wi-Fi (WPA2), HTTP(S) + X-API-Key--> [Pi: FastAPI] <-- [Dashboard/Browser]
                                 |  ^                                              |
                        Relay -> Pump  +--- Response: config / commands / demo ----+
                                 ^
                    USB (serial console, firmware upload), BOOT button
```

Trust boundaries:

1. **Wi-Fi** between the ESP32 and the Pi (radio link, can in principle be eavesdropped on)
2. **Server response**: the ESP32 executes commands that someone triggered in the dashboard.
3. **Physical access**: USB cable and BOOT button on the planter

## 3. Threats (STRIDE) and countermeasures

| # | Threat | STRIDE | Countermeasure in the ESP32 | Residual risk |
|---|---|---|---|---|
| T1 | Someone sends a flood of pump commands via the dashboard or a fake server | Tampering / Elevation of privilege | Every start goes through `pumpStart()`: at most 15 s per run, **a minimum pause of 10 s before every start**, a daily limit of at most 300 s, and dry-run protection. These limits are compiled into the firmware and cannot be changed by the server. | Watering is still possible within these limits. Admin authentication on the server is the responsibility of the server/dashboard. |
| T2 | Manipulated `config` (e.g. daily limit 10,000 s, pause 0 s, tank 50 l) | Tampering | `configClampSettings()` clamps **every** value, including those loaded from flash. NaN is replaced by the default value. | Values within the permitted ranges remain possible, e.g. a high target moisture. The physical tank size limits the amount of water. |
| T3 | Bypassing dry-run protection via the demo fill level `water_level_pct: 100` | Tampering | The **demo fill level is display-only** and is not stored. The protection uses `min(real, demo)`: a demo can only make the tank look emptier, never fuller. After the demo, the real estimate applies again. | – |
| T4 | Bypassing dry-run protection via the `tank_refilled` command | Tampering | The ESP32 cannot verify whether the tank was actually refilled. Every source is logged (`[TANK] ... (Dashboard)`). The daily limit caps the pump running time. | **Remains** → the server must protect this command behind an admin login (issue filed for the web team). |
| T5 | Demo values outside any realistic range (e.g. temperature 1e9, NaN) | Tampering | Value ranges per field (e.g. temperature −20…60 °C). NaN and Inf are discarded. Demos run for at most 600 s. JSON output goes through `putNumber()`, so `nan` never appears in the JSON. | – |
| T6 | Eavesdropping on the API key over Wi-Fi | Information disclosure | Wi-Fi uses WPA2. HTTPS is prepared (`https://` URL; with `SERVER_CA_CERT` including certificate validation). The key is never printed to the serial console. | As long as the server only speaks HTTP, anyone on the Wi-Fi network can read the key. → Set up Caddy with TLS. |
| T7 | Fake server (same SSID, attacker's own Pi) | Spoofing | With `SERVER_CA_CERT`, the ESP32 only accepts the genuine server. | Without a certificate, or over HTTP, the risk remains. Its impact is limited by T1/T2. |
| T8 | Password or API key ends up in the public GitHub repository | Information disclosure | Optional `secrets.h` (listed in `.gitignore`), placeholders in the sketch, warning at startup. `WiFi.persistent(false)`: the credentials are not additionally stored in the Wi-Fi flash area. | Anyone who writes the credentials into the sketch anyway and commits it publishes them → check `git diff` before every commit. |
| T9 | Firmware hangs, relay stays on | Denial of service | Non-blocking code, no HTTP while the pump is running, all network timeouts 4 s (including the TLS handshake). **Watchdog** after 20 s: reboot, and `pumpBegin()` switches the relay off first. After every boot, automatic watering waits for the configured pause so that a reboot loop cannot pump continuously. In addition, every `loop()` iteration checks that the relay is off when no pump run is active. | – |
| T10 | Sensor defective, disconnected or miscalibrated → continuous watering | (Safety) | Plausibility limits for the raw value (100…4000). An invalid calibration (dry − wet < 300) is not saved and is treated as a sensor error. On a sensor error there is **no** automatic watering. | – |
| T11 | Sensor not in the soil, hose lying next to the pot → water is wasted | (Safety / sustainability) | **Plausibility check**: if, within one watering session, at least 3 runs with a total of at least 5 s pumping time (over at least 3 min) raise the moisture by less than 3 percentage points, the ESP32 blocks automatic watering and raises an alarm. With the default burst of 0.5 s this means the check applies after about 10 runs. | The check is suspended in demo mode (values are fixed there). |
| T12 | Attacker with physical access (USB, BOOT button) | Elevation of privilege | The serial console is only reachable via cable. All pump limits apply there as well. | With physical access, new firmware can be flashed. Accepted for a school planter. Countermeasures would be Secure Boot and flash encryption. |
| T13 | Server returns huge or malformed responses | Denial of service | HTTP timeouts of 4 s. Malformed JSON is discarded, error texts are truncated to 200 characters. If memory problems cause a hang, the watchdog takes over. | – |

## 4. Mapping to the OWASP IoT Top 10 (2018)

| OWASP IoT | Implementation in the ESP32 |
|---|---|
| I1 Weak, guessable or hardcoded passwords | No default passwords in the firmware, only placeholders with a warning. The server generates the API key per device. |
| I2 Insecure network services | The ESP32 offers **no** services: no web server, no OTA, no Telnet. It only establishes outgoing connections. |
| I3 Insecure ecosystem interfaces | The API key is sent with every request. Securing the dashboard commands is the server's responsibility (see T4). |
| I4 Lack of secure update mechanism | Updates are deliberately possible via USB only. OTA would add attack surface. |
| I5 Use of insecure or outdated components | Current ESP32 Arduino core 3.3.x, ArduinoJson 7, Adafruit DHT; no libraries of unclear origin (the LED bar driver is written in-house). |
| I6 Insufficient privacy protection | Only environmental data is collected, no personal data. |
| I7 Insecure data transfer and storage | Transfer via WPA2, HTTPS is prepared. Flash only contains settings, calibration and the tank level; no credentials are kept in the Wi-Fi storage area. |
| I8 Lack of device management | Device ID and firmware version are included in every message. The server sees `uptime_s`, `rssi_dbm` and error codes. |
| I9 Insecure default settings | The defaults are conservative: 0.5 s per run, 300 s pause, 60 s per day. |
| I10 Lack of physical hardening | See T12. The electronics must be protected from splash water ([verkabelung.md](verkabelung.md)). |

## 5. Recommendations for the server/dashboard (from the review)

Only the server side can address these points. They are described in detail in the GitHub issue for the web team.

1. **Admin authentication** for all control endpoints (pump, `tank_refilled`, demo). This is the most important measure; it closes T1 and T4.
2. Provide **TLS** (Caddy `tls internal`) and the root certificate for `SERVER_CA_CERT`. This closes T6 and T7.
3. Make Uvicorn reachable only within the hotspot network (bind to `10.42.0.1` or use a firewall).
4. Apply a rate limit before the API key check.

## 6. Review log, firmware v0.2.4 → v0.3.0

| Finding in v0.2.4 | Severity | Fixed in v0.3.0 |
|---|---|---|
| The demo fill level was **permanently** saved as the real tank estimate. "Tank 100" via demo disabled dry-run protection; "tank 4" blocked the pump even after the demo. | High | Demo fill level is now display-only; protection uses `min(real, demo)` |
| Dashboard, demo and serial pump commands ignored any pause. Many commands in a row resulted in continuous operation up to the daily limit. | High | Hard minimum pause of 10 s before every start |
| A miscalibration (dry ≈ wet) resulted in 0 % moisture and thus continuous watering up to the daily limit. | High | Calibration is validated; invalid = sensor error = no watering |
| No protection if the sensor is not in the soil | Medium | Plausibility check with blocking and alarm |
| No watchdog: if the firmware hung while the pump was running, the pump kept running until the next reboot. | Medium | 20 s task watchdog, relay safety net in `pumpUpdate()` |
| `ledtest`/`ledswap` blocked for 3 s with `delay()`, even while the pump was running | Medium | Test animation runs in the background |
| Demo temperature without limits. NaN would have produced invalid JSON (`nan`). | Low | Value ranges per field; `putNumber()` writes `null` |
| `pump_cooldown_s` without an upper bound: possible overflow in `s * 1000` | Low | Upper bound of 86,400 s |
| Soil raw value ≥ 4000 (short circuit to 3V3) was accepted as valid, interpreted as 0 %, and triggered watering | Low | Plausibility range 100…4000 |
| Wi-Fi credentials additionally stored in the Wi-Fi flash area | Low | `WiFi.persistent(false)` |
| Password only in the sketch: risk of committing it to the public repository | Low | Optional `secrets.h`, warning when placeholders are present |
| Implausible DHT values were accepted | Low | Range check |
| *Second review round (independent review of the changes):* | | |
| A hanging TLS handshake (default timeout 120 s) would have triggered the watchdog. In a reboot loop, watering would then have started immediately after every boot, and the daily limit was held in RAM only. | High | Handshake timeout of 4 s. After every boot, automatic watering first waits `pump_cooldown_s`. The reset reason is logged. |
| A demo soil value could lift the watering block. A demo session continued with real values, and the plausibility check then compared against the demo start value. | Medium | Only a real value can lift the block. Switching between demo and real values ends the session. |
| Demo immediate watering ignored `auto_water = false` and the watering block | Medium | Demo watering only when automatic watering is enabled and not blocked |
| `cal light` also saved an invalid soil calibration pair | Low | Saving only happens if the soil pair is valid |
| `demo soil nan` (serial) | Low | Non-finite values are rejected |
| Display timer wrong after 24.8 days of uptime (timestamp comparison) | Low | Flag + start time instead of end time |
| Watchdog no longer monitored the Wi-Fi idle task; setup errors were not reported | Low | Core 0 idle task is monitored as well, status in the log (`[WDT]`) |

Checked and found to be in order:

- Overflow-safe `millis()` comparisons
- Relay is off at startup before the pin becomes an output
- No HTTP while the pump is running
- No password and no key in the log
- No open network services
- Values from the server are clamped
- The firmware compiles without warnings (`--warnings all`)
