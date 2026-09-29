# Test tools

## `fake_esp.py` – ESP32 simulator

`fake_esp.py` lets you develop and test the server and dashboard without any hardware. It sends readings in exactly the format of the API contract ([`.claude/api-contract.md`](../.claude/api-contract.md), v1.2) to the server and processes the response (`commands`, `config`, `demo`). It reports the firmware version `sim-0.3.0` and behaves like the real firmware v0.3.0:

- Soil moisture dries out depending on time of day, temperature and light. Day and night are simulated, including the light and temperature curves.
- Automatic watering with the same safety limits as the firmware: at most 15 s per run, a daily limit of at most 300 s, a 10 s minimum pause before every start, and dry-run protection (no pumping at a tank level of 5 % or less).
- An estimated water tank. A demo fill level is for display only and never makes the tank "fuller" for the dry-run protection.
- Demo mode with value overrides, forced sensor errors and expiry time.
- Commands from the server (manual watering, tank refilled) are applied.

It only needs the Python standard library (3.9 or newer); nothing has to be installed.

### Examples

```bash
# Local server, API key from the server
python tools/fake_esp.py --url http://localhost:8000 --key <api-key>

# Time-lapse: send every 2 s, 1 s = 60 simulated seconds
python tools/fake_esp.py --url http://localhost:8000 --key <api-key> --interval 2 --speed 60

# Test anomalies for the AI (starting with message 6)
python tools/fake_esp.py --url http://localhost:8000 --key <api-key> --anomaly heat --count 20
python tools/fake_esp.py --url http://localhost:8000 --key <api-key> --anomaly dht

# Only show the JSON, send nothing
python tools/fake_esp.py --dry-run --count 3

# HTTPS with a self-signed certificate (Caddy tls internal)
python tools/fake_esp.py --url https://10.42.0.1 --key <api-key> --insecure
```

With `--speed 60`, drying out and the day/night cycle become visible within a few minutes.

### Options

| Option | Default | Meaning |
|---|---|---|
| `--url` | `http://localhost:8000` | Base URL of the server |
| `--key` | `dev-key` | API key (header `X-API-Key`) |
| `--device-id` | `esp32-kuebel-01` | Device ID |
| `--interval` | from the server config | Send interval in seconds (until the server has sent a config, the simulator's built-in default of 15 s is used) |
| `--speed` | `1` | Time-lapse factor (simulated seconds per real second) |
| `--count` | `0` (endless) | Number of messages |
| `--start-moisture` / `--start-hour` | `45` / `10` | Start values of the simulation (soil moisture in %, simulated time of day) |
| `--anomaly` | `none` | `heat` = +15 °C, `dht` = sensor failure (both from message 6 on) |
| `--seed` | random | For reproducible values |
| `--insecure` | off | Do not verify the HTTPS certificate (testing only!) |
| `--dry-run` | off | Send nothing, print the JSON |

Do not put real API keys into scripts or into a shell history that could end up in the repository.
