# Test-Tools

## `fake_esp.py` – simuliert den ESP32

Das Skript sendet Messwerte genau im Format des API-Vertrags (`.claude/api-contract.md`, v1.2) an den Server und wertet die Antwort aus (`commands`, `config`, `demo`). Es verhält sich wie die echte Firmware v0.3.0:

- Die Bodenfeuchte trocknet aus, abhängig von Tageszeit, Temperatur und Licht. Tag und Nacht werden simuliert.
- Auto-Bewässerung mit denselben Sicherheitsgrenzen: höchstens 15 s pro Lauf, Tageslimit höchstens 300 s, 10 s Mindestpause, Trockenlaufschutz.
- Geschätzter Tank. Ein Demo-Füllstand ist nur Anzeige und macht den Tank für den Trockenlaufschutz nie „voller“.
- Demo-Modus mit Overrides, erzwungenen Fehlern und Ablaufzeit.

Es braucht nur die Python-Standardbibliothek (ab 3.9), keine Installation.

```bash
# Server lokal, API-Key vom Server
python tools/fake_esp.py --url http://localhost:8000 --key <api-key>

# Zeitraffer: alle 2 s senden, 1 s = 60 simulierte s
python tools/fake_esp.py --url http://localhost:8000 --key <api-key> --interval 2 --speed 60

# Anomalien für die KI testen (ab Nachricht 6)
python tools/fake_esp.py --url http://localhost:8000 --key <api-key> --anomaly heat --count 20
python tools/fake_esp.py --url http://localhost:8000 --key <api-key> --anomaly dht

# Nur das JSON ansehen, nichts senden
python tools/fake_esp.py --dry-run --count 3

# HTTPS mit selbstsigniertem Zertifikat (Caddy tls internal)
python tools/fake_esp.py --url https://10.42.0.1 --key <api-key> --insecure
```

| Option | Standard | Bedeutung |
|---|---|---|
| `--url` | `http://localhost:8000` | Basis-URL des Servers |
| `--key` | `dev-key` | API-Key (Header `X-API-Key`) |
| `--device-id` | `esp32-kuebel-01` | Geräte-ID |
| `--interval` | aus Server-Config | Sendeintervall in Sekunden |
| `--speed` | `1` | Zeitraffer (simulierte Sekunden pro echter Sekunde) |
| `--count` | `0` (endlos) | Anzahl Nachrichten |
| `--start-moisture` / `--start-hour` | `45` / `10` | Startwerte der Simulation |
| `--anomaly` | `none` | `heat` = +15 °C, `dht` = Sensorausfall |
| `--seed` | zufällig | für reproduzierbare Werte |
| `--insecure` | aus | HTTPS-Zertifikat nicht prüfen (nur im Test!) |
| `--dry-run` | aus | nichts senden, JSON ausgeben |

Echte API-Keys nicht in Skripte oder die Shell-History schreiben, die im Repo landen.
