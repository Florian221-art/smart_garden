// ============================================================================
//  EINSTELLUNGEN – HIER ANPASSEN
//  (Achtung: Das GitHub-Repo ist öffentlich – echtes WLAN-Passwort/API-Key
//   nicht committen. Vor "git pull": "git stash", danach "git stash pop".)
// ============================================================================

// --- WLAN + Server --------------------------------------------------------
const char *CFG_WIFI_SSID     = "SmartGarden";            // WLAN-Name (Hotspot des Raspberry Pi)
const char *CFG_WIFI_PASSWORD = "hier-passwort";          // WLAN-Passwort (von Nico)
const char *CFG_SERVER_URL    = "http://10.42.0.1:8000";  // Server auf dem Pi (später: "https://10.42.0.1")
const char *CFG_API_KEY       = "hier-api-key";           // API-Key des Geräts (vom Server)
const char *CFG_DEVICE_ID     = "esp32-kuebel-01";        // Name des Geräts

// --- LED-Bar (Bodenfeuchte) -----------------------------------------------
// Segment 1 der Bar ist rot, Segment 2 orange, Segmente 3-10 grün.
int  CFG_LEDBAR_MODE    = 2;      // 2 = Trockenheitsbalken ab Grün (Standard): feucht = wenige grüne LEDs,
                                  //     je trockener, desto mehr grüne -> dann orange -> ganz trocken auch rot
                                  // 0 = Zeiger: nur 2 LEDs an der Position des Werts (trocken = rot, feucht = grün)
                                  // 1 = Füllbalken ab Rot (trocken = nur rot, feucht = ganzer Balken)
bool CFG_LEDBAR_REVERSE = false;  // true = Anzeige spiegeln, falls rot/grün vertauscht erscheint
bool CFG_LEDBAR_SWAP_PINS = false; // true, wenn die Bar gar nicht reagiert (DI/DCKI = D18/D19 vertauscht)

// --- Summer ----------------------------------------------------------------
bool CFG_BUZZER_ENABLED = true;   // false = Summer komplett stumm

// ============================================================================

/*
  Smart Garden – ESP32-Firmware
  Hackathon Euregio 2026 · Team Florian Schoenen & Nico Steins

  Misst Bodenfeuchte (kapazitiv), Licht, Temperatur und Luftfeuchte, zeigt die
  Bodenfeuchte auf der Grove LED Bar (rot = trocken, grün = feucht), bewässert
  selbstständig über Relais + Pumpe und schickt alles an den Server auf dem Pi.

  Aufbau/Verkabelung: docs/hardware/verkabelung.md
  Schnittstelle:      .claude/api-contract.md (v1.2)
  Anleitung:          firmware/smart_garden/README.md

  Serielle Konsole (115200 Baud, Zeilenende "Neue Zeile"): "help" eingeben.
*/

#include <ArduinoJson.h>
#include <WiFi.h>
#include "garden_config.h"
#include "sensors.h"
#include "tank.h"
#include "pump.h"
#include "demo.h"
#include "display.h"
#include "garden_net.h"

// Vorwärtsdeklarationen
void setup();
void loop();
static void measureAndAct();
static void printStatus();
static void applyConfig(JsonObjectConst cfg);
static void handleCommands(JsonObjectConst cmd);
static void sendReading();
static void printHelp();
static void handleSerialCommand(String line);
static void pollSerial();
static void pollBootButton();

static Readings current;
static uint32_t seq = 0;
static uint32_t lastSensorMs = 0;
static uint32_t lastSendMs = 0;
static bool autoWateredSinceLast = false;
static bool wateringSession = false;  // gießen, bis moisture_target_pct erreicht ist
static uint32_t bootPressedSince = 0;
static bool bootHandled = false;
static String serialLine;

// ---------------------------------------------------------------------------
// Messen + lokale Logik
// ---------------------------------------------------------------------------
static void measureAndAct() {
  current = sensorsRead();
  demoApply(current);

  // LED-Bar + Tankanzeige
  displaySetSoil(current.soil_pct, current.soil_ok);
  displaySetTankEmpty(tankIsEmpty());

  // Alarm (Summer) bei kritischen Zuständen. Sensorfehler zählen erst, wenn sie
  // länger als 60 s anhalten (DHT11 setzt gern mal kurz aus). Siehe buzzerAlarm().
  static uint32_t dhtBadSince = 0, soilBadSince = 0;
  uint32_t nowMs = millis();
  if (current.dht_ok) dhtBadSince = 0; else if (!dhtBadSince) dhtBadSince = nowMs;
  if (current.soil_ok) soilBadSince = 0; else if (!soilBadSince) soilBadSince = nowMs;
  bool demoErr = demoActive();  // im Demo-Modus Fehler sofort melden
  uint8_t problems = 0;
  if (tankIsEmpty()) problems |= 1;
  if (soilBadSince && (demoErr || nowMs - soilBadSince > 60000)) problems |= 2;
  if (dhtBadSince && (demoErr || nowMs - dhtBadSince > 60000)) problems |= 4;
  buzzerAlarm(problems);

  // Demo: neue Bodenfeuchte-Vorgabe unter dem Grenzwert -> sofort einmal gießen (ohne Pause)
  if (demoTakeSoilKick() && current.soil_ok && current.soil_pct < settings.moisture_min_pct && !pumpRunning()) {
    if (pumpStart(settings.max_pump_s_per_run, "Demo", false)) {
      autoWateredSinceLast = true;
      wateringSession = true;
      return;
    }
  }

  // Automatische Bewässerung
  if (!settings.auto_water || !current.soil_ok || pumpRunning()) return;
  if (tankIsEmpty() || pumpTodaySeconds() >= settings.max_pump_s_per_day) {  // Trockenlaufschutz / Tageslimit
    static uint32_t lastBlockMsg = 0;
    if (current.soil_pct < settings.moisture_min_pct && (lastBlockMsg == 0 || millis() - lastBlockMsg > 30000)) {
      lastBlockMsg = millis();
      if (tankIsEmpty())
        Serial.printf("[PUMPE] Auto-Gießen blockiert: Tank leer (geschätzt %.0f ml) -> \"refill\" eingeben oder BOOT 3 s halten\n",
                      tankRemainingMl());
      else
        Serial.println("[PUMPE] Auto-Gießen blockiert: Tageslimit erreicht");
    }
    wateringSession = false;
    return;
  }
  if (current.soil_pct < settings.moisture_min_pct) wateringSession = true;
  if (current.soil_pct >= settings.moisture_target_pct) wateringSession = false;
  if (wateringSession && pumpCooldownActive()) {
    static uint32_t lastCdMsg = 0;
    if (lastCdMsg == 0 || millis() - lastCdMsg > 30000) {
      lastCdMsg = millis();
      Serial.printf("[PUMPE] Erde trocken, Pause nach letztem Gießen: noch %lu s\n", (unsigned long)pumpCooldownLeftS());
    }
  }
  if (wateringSession && !pumpCooldownActive()) {
    if (pumpStart(settings.max_pump_s_per_run, "auto", true)) {
      autoWateredSinceLast = true;
    } else {
      wateringSession = false;  // Tank leer / Tageslimit -> Sitzung beenden
    }
  }
}

static void printStatus() {
  Serial.printf("[MESS] Boden %s%.1f %% (roh %d)  Licht %s%.1f %% (roh %d)  ", current.demo_soil ? "[DEMO]" : "",
                current.soil_pct, current.soil_raw, current.demo_light ? "[DEMO]" : "", current.light_pct,
                current.light_raw);
  if (current.dht_ok)
    Serial.printf("Temp %s%.1f °C  LF %s%.0f %%  ", current.demo_temp ? "[DEMO]" : "", current.temp_c,
                  current.demo_hum ? "[DEMO]" : "", current.hum_pct);
  else
    Serial.print("DHT11 FEHLER  ");
  Serial.printf("Tank %.0f ml (%.0f %%)  Pumpe %s  WLAN %s\n", tankRemainingMl(), tankLevelPct(),
                pumpRunning() ? "AN" : "aus", netConnected() ? "ok" : "--");
}

// ---------------------------------------------------------------------------
// Server
// ---------------------------------------------------------------------------
static void applyConfig(JsonObjectConst cfg) {
  if (cfg.isNull()) return;
  Settings s = settings;
  s.interval_s = cfg["interval_s"] | s.interval_s;
  s.moisture_min_pct = cfg["moisture_min_pct"] | s.moisture_min_pct;
  s.moisture_target_pct = cfg["moisture_target_pct"] | s.moisture_target_pct;
  s.auto_water = cfg["auto_water"] | s.auto_water;
  s.max_pump_s_per_run = cfg["max_pump_s_per_run"] | s.max_pump_s_per_run;
  s.pump_cooldown_s = cfg["pump_cooldown_s"] | s.pump_cooldown_s;
  s.max_pump_s_per_day = cfg["max_pump_s_per_day"] | s.max_pump_s_per_day;
  s.buzzer_enabled = cfg["buzzer_enabled"] | s.buzzer_enabled;
  s.tank_capacity_ml = cfg["tank_capacity_ml"] | s.tank_capacity_ml;
  s.pump_flow_ml_per_s = cfg["pump_flow_ml_per_s"] | s.pump_flow_ml_per_s;
  s.tank_low_pct = cfg["tank_low_pct"] | s.tank_low_pct;
  configClampSettings(s);
  if (memcmp(&s, &settings, sizeof(Settings)) != 0) {
    settings = s;
    configSaveSettings();
    Serial.println("[CFG] neue Einstellungen vom Server übernommen");
    configPrint();
  }
}

static void handleCommands(JsonObjectConst cmd) {
  if (cmd.isNull()) return;
  if (cmd["tank_refilled"] | false) {
    tankRefill();
    buzzerBeep(2);
  }
  float pumpS = cmd["pump_run_s"] | 0.0f;
  if (pumpS > 0) pumpStart(pumpS, "manuell (Dashboard)", false);
  if (cmd["buzzer"] | false) {
    if (settings.buzzer_enabled) buzzerBeep(3);
  }
  if (cmd["identify"] | false) {
    displayIdentify();
    buzzerBeep(1);
  }
}

static void sendReading() {
  JsonDocument doc;
  doc["device_id"] = CFG_DEVICE_ID;
  doc["fw_version"] = FW_VERSION;
  doc["seq"] = seq;
  doc["uptime_s"] = millis() / 1000;
  doc["rssi_dbm"] = WiFi.RSSI();

  if (current.soil_ok) {
    doc["soil_moisture_pct"] = serialized(String(current.soil_pct, 1));
    doc["soil_moisture_raw"] = current.soil_raw;
  } else {
    doc["soil_moisture_pct"] = nullptr;
    doc["soil_moisture_raw"] = nullptr;
  }
  if (current.light_ok) {
    doc["light_pct"] = serialized(String(current.light_pct, 1));
    doc["light_raw"] = current.light_raw;
  } else {
    doc["light_pct"] = nullptr;
    doc["light_raw"] = nullptr;
  }
  if (current.dht_ok) {
    doc["air_temp_c"] = serialized(String(current.temp_c, 1));
    doc["air_humidity_pct"] = serialized(String(current.hum_pct, 1));
  } else {
    doc["air_temp_c"] = nullptr;
    doc["air_humidity_pct"] = nullptr;
  }
  doc["tank_remaining_ml"] = serialized(String(tankRemainingMl(), 1));
  doc["water_level_pct"] = serialized(String(tankLevelPct(), 1));
  float pumpS = pumpTakeOnSecondsSinceLast();
  doc["pump_on_s_since_last"] = serialized(String(pumpS, 1));
  doc["pump_running"] = pumpRunning();
  doc["auto_water_triggered"] = autoWateredSinceLast;

  JsonArray errors = doc["errors"].to<JsonArray>();
  if (!current.soil_ok) errors.add("soil_out_of_range");
  if (!current.light_ok) errors.add("light_read_failed");
  if (!current.dht_ok) errors.add("dht_read_failed");
  if (tankIsEmpty()) errors.add("tank_empty");

  JsonArray demoFields = doc["demo_overrides"].to<JsonArray>();
  demoOverriddenFields(demoFields);

  JsonDocument resp;
  int code = netPostReading(doc, resp);
  if (code == 200) {
    seq++;
    autoWateredSinceLast = false;
    applyConfig(resp["config"].as<JsonObjectConst>());
    handleCommands(resp["commands"].as<JsonObjectConst>());
    demoApplyFromServer(resp["demo"]);
    Serial.printf("[HTTP] #%lu gesendet -> 200\n", (unsigned long)(seq - 1));
  } else {
    pumpRestoreOnSeconds(pumpS);  // Laufzeit beim nächsten Mal mitschicken
    seq++;
    netMarkError();
  }
}

// ---------------------------------------------------------------------------
// Serielle Konsole
// ---------------------------------------------------------------------------
static void printHelp() {
  Serial.println(F(
      "Befehle:\n"
      "  status                 aktuelle Werte + Einstellungen\n"
      "  demo soil|light|temp|hum|tank <wert> [sekunden]   Wert überschreiben (Standard 120 s)\n"
      "  demo error dht|soil|light [sekunden]               Sensorausfall simulieren\n"
      "  demo off               Demo beenden\n"
      "  pump <sekunden>        Pumpe manuell (Sicherheitsgrenzen gelten)\n"
      "  stop                   Pumpe sofort aus\n"
      "  refill                 Tank als aufgefüllt markieren\n"
      "  cal soil dry|wet       aktuellen Rohwert als trocken/nass speichern\n"
      "  cal light dark|bright  aktuellen Rohwert als dunkel/hell speichern\n"
      "  ledtest | ledflip      LED-Bar testen / Richtung umdrehen (bis Neustart)\n"
      "  ledseg <0-10>          genau so viele Segmente 10 s lang anzeigen (Test)\n"
      "  ledswap                Daten-/Taktpin der LED-Bar tauschen (wenn sie nicht reagiert)\n"
      "  mute                   Summer an/aus (bis Neustart)\n"
      "  beep                   Summer testen\n"
      "  send                   sofort an Server senden"));
}

static void handleSerialCommand(String line) {
  line.trim();
  if (line.isEmpty()) return;
  String parts[4];
  int n = 0, start = 0;
  for (int i = 0; i <= (int)line.length() && n < 4; i++) {
    if (i == (int)line.length() || line[i] == ' ') {
      if (i > start) parts[n++] = line.substring(start, i);
      start = i + 1;
    }
  }
  String c = parts[0];
  c.toLowerCase();

  if (c == "help" || c == "?") printHelp();
  else if (c == "status") { printStatus(); configPrint(); demoPrint(); }
  else if (c == "demo" && parts[1] == "off") demoOff();
  else if (c == "demo" && parts[1] == "error" && n >= 3)
    demoForceError(parts[2], n >= 4 ? parts[3].toInt() : 120);
  else if (c == "demo" && n >= 3) {
    demoSetValue(parts[1], parts[2].toFloat(), n >= 4 ? parts[3].toInt() : 120);
    measureAndAct();
  }
  else if (c == "pump" && n >= 2) pumpStart(parts[1].toFloat(), "manuell (seriell)", false);
  else if (c == "stop") pumpStop();
  else if (c == "refill") { tankRefill(); buzzerBeep(2); }
  else if (c == "cal" && n >= 3) {
    if (parts[1] == "soil") {
      int raw = sensorsReadSoilRaw();
      if (parts[2] == "dry") calib.soil_raw_dry = raw;
      else if (parts[2] == "wet") calib.soil_raw_wet = raw;
      Serial.printf("[CAL] Boden %s = %d\n", parts[2].c_str(), raw);
    } else if (parts[1] == "light") {
      int raw = sensorsReadLightRaw();
      if (parts[2] == "dark") calib.light_raw_dark = raw;
      else if (parts[2] == "bright") calib.light_raw_bright = raw;
      Serial.printf("[CAL] Licht %s = %d\n", parts[2].c_str(), raw);
    }
    configSaveCalibration();
  }
  else if (c == "ledtest") displayLedTest();
  else if (c == "ledflip") displayFlip();
  else if (c == "ledseg" && n >= 2) displayTestSegments(parts[1].toInt());
  else if (c == "ledswap") displaySwapPins();
  else if (c == "mute") { CFG_BUZZER_ENABLED = !CFG_BUZZER_ENABLED; Serial.printf("[SUMMER] %s\n", CFG_BUZZER_ENABLED ? "an" : "stumm"); }
  else if (c == "beep") buzzerBeepForced(2);
  else if (c == "send") lastSendMs = 0;
  else Serial.println("Unbekannter Befehl – \"help\" eingeben");
}

static void pollSerial() {
  while (Serial.available()) {
    char ch = (char)Serial.read();
    if (ch == '\n' || ch == '\r') {
      if (serialLine.length()) handleSerialCommand(serialLine);
      serialLine = "";
    } else if (serialLine.length() < 80) {
      serialLine += ch;
    }
  }
}

// BOOT-Taste 3 s halten = Tank aufgefüllt
static void pollBootButton() {
  if (digitalRead(PIN_BOOT_BTN) == LOW) {
    if (bootPressedSince == 0) bootPressedSince = millis();
    if (!bootHandled && millis() - bootPressedSince > 3000) {
      bootHandled = true;
      tankRefill();
      buzzerBeep(2);
    }
  } else {
    bootPressedSince = 0;
    bootHandled = false;
  }
}

// ---------------------------------------------------------------------------
void setup() {
  pumpBegin();  // zuerst: Relais sicher AUS
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.printf("=== Smart Garden ESP32 – Firmware %s – Gerät %s ===\n", FW_VERSION, CFG_DEVICE_ID);

  pinMode(PIN_BOOT_BTN, INPUT_PULLUP);
  configBegin();
  tankBegin();
  sensorsBegin();
  displayBegin();
  configPrint();
  displayLedTest();
  buzzerBeep(1);
  netBegin();
  printHelp();
  if (tankIsEmpty())
    Serial.printf("[TANK] Achtung: Tank laut Schätzung leer (%.0f ml) – Pumpe gesperrt. \"refill\" eingeben, wenn aufgefüllt.\n",
                  tankRemainingMl());
  lastSendMs = millis() - settings.interval_s * 1000UL + 5000;  // erste Nachricht nach ~5 s
}

void loop() {
  uint32_t now = millis();

  pumpUpdate();  // Pumpe immer zuerst: Laufzeitende nie verpassen
  netUpdate();
  displayUpdate();
  pollSerial();
  pollBootButton();

  if (now - lastSensorMs >= SENSOR_PERIOD_MS) {
    lastSensorMs = now;
    measureAndAct();
  }

  // nicht senden, während die Pumpe läuft (HTTP blockiert bis zu 4 s)
  if (!pumpRunning() && now - lastSendMs >= settings.interval_s * 1000UL) {
    lastSendMs = now;
    printStatus();
    if (netConnected()) sendReading();
    else Serial.println("[HTTP] kein WLAN – Senden übersprungen");
  }
}
