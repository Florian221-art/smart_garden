// ############################################################################
//  FIRMWARE-VERSION 0.3.4  (Stand 29.09.2026)
//  Muss mit FW_VERSION in garden_config.h übereinstimmen und steht beim Start
//  im seriellen Monitor. So siehst du sofort, ob du die aktuelle Datei hast.
// ############################################################################

// ============================================================================
//  EINSTELLUNGEN – HIER ANPASSEN
//  Achtung: Das GitHub-Repo ist öffentlich. Echtes WLAN-Passwort und API-Key
//  NICHT committen. Sicherer: in eine Datei secrets.h schreiben (wird von Git
//  ignoriert, Vorlage secrets.h.example) – deren Werte haben Vorrang.
//  Vor "git pull" lokale Änderungen mit "git stash" sichern, danach "git stash pop".
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
  Bodenfeuchte auf der Grove LED Bar, bewässert selbstständig über Relais +
  Pumpe und schickt alle Werte an den Server auf dem Raspberry Pi.

  Ablauf (alles nicht blockierend, gesteuert über millis()):
    loop()
     ├─ pumpUpdate()      Pumpe nach Ablauf der Laufzeit ausschalten (immer zuerst)
     ├─ netUpdate()       WLAN halten, Status-LED
     ├─ displayUpdate()   LED-Bar, Summer, Blinken
     ├─ pollSerial()      Befehle aus dem seriellen Monitor
     ├─ pollBootButton()  BOOT 3 s = Tank aufgefüllt
     ├─ alle 2 s:  measureAndAct()   messen, Demo anwenden, Alarm, Auto-Gießen
     └─ alle interval_s: sendReading()  POST an den Server, Antwort auswerten

  Sicherheit (Details: docs/hardware/sicherheit-esp.md):
    - Harte Pumpengrenzen in garden_config.h, vom Server nicht änderbar
    - Trockenlaufschutz über die Tank-Schätzung
    - Plausibilitätsprüfung: wirkt das Gießen nicht, wird Auto-Gießen gesperrt
    - Watchdog: hängt die Firmware, startet der ESP neu -> Relais AUS
    - Passwort und API-Key werden nie ausgegeben

  Aufbau/Verkabelung: docs/hardware/verkabelung.md
  Schnittstelle:      .claude/api-contract.md (v1.2)
  Anleitung:          firmware/smart_garden/README.md

  Serielle Konsole (115200 Baud, Zeilenende "Neue Zeile"): "help" eingeben.
*/

#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include "garden_config.h"
#include "sensors.h"
#include "tank.h"
#include "pump.h"
#include "demo.h"
#include "display.h"
#include "garden_net.h"

// Vorwärtsdeklarationen (die Arduino-IDE erzeugt sie nicht zuverlässig)
void setup();
void loop();
static void measureAndAct();
static void autoWater();
static void clearWateringBlock(const char *why);
static void printStatus();
static void applyConfig(JsonObjectConst cfg);
static void handleCommands(JsonObjectConst cmd);
static void putNumber(JsonDocument &doc, const char *key, float value, bool valid);
static void sendReading();
static void printHelp();
static void calibrateSoil(const String &which);
static void handleSerialCommand(String line);
static void pollSerial();
static void pollBootButton();
static void watchdogBegin();
static void printResetReason();

static Readings current;                  // letzte Messung (inkl. Demo-Werten)
static uint32_t seq = 0;                  // laufende Nummer jeder Nachricht
static uint32_t lastSensorMs = 0;
static uint32_t lastSendMs = 0;
static bool autoWateredSinceLast = false; // für "auto_water_triggered"
static uint32_t bootPressedSince = 0;
static bool bootHandled = false;
static String serialLine;

// Gieß-Sitzung: unter moisture_min_pct beginnt sie, bei moisture_target_pct endet sie
static bool wateringSession = false;
static float sessionStartSoil = 0;        // Bodenfeuchte beim Start der Sitzung
static uint32_t sessionStartMs = 0;
static uint8_t sessionRuns = 0;           // Pumpenläufe in dieser Sitzung
static bool sessionDemo = false;          // Sitzung mit Demo-Bodenwert gestartet?
static bool wateringBlocked = false;      // Gießen wirkt nicht -> Auto-Gießen gesperrt

// ---------------------------------------------------------------------------
// Messen + lokale Logik (alle SENSOR_PERIOD_MS)
// ---------------------------------------------------------------------------
static void measureAndAct() {
  current = sensorsRead();
  demoApply(current);

  displaySetSoil(current.soil_pct, current.soil_ok);
  displaySetTankEmpty(tankIsEmpty());

  // Alarm (Summer) bei kritischen Zuständen. Sensorfehler zählen erst, wenn sie
  // länger als 60 s anhalten (der DHT11 setzt gern mal kurz aus) – im Demo-Modus
  // sofort, damit man es vorführen kann.
  static uint32_t dhtBadSince = 0, soilBadSince = 0;
  uint32_t nowMs = millis();
  if (current.dht_ok) dhtBadSince = 0;
  else if (!dhtBadSince) dhtBadSince = nowMs;
  if (current.soil_ok) soilBadSince = 0;
  else if (!soilBadSince) soilBadSince = nowMs;
  bool demoNow = demoActive();
  uint8_t problems = 0;
  if (tankIsEmpty()) problems |= ALARM_TANK_EMPTY;
  if (soilBadSince && (demoNow || nowMs - soilBadSince > 60000)) problems |= ALARM_SOIL_SENSOR;
  if (dhtBadSince && (demoNow || nowMs - dhtBadSince > 60000)) problems |= ALARM_DHT;
  if (wateringBlocked) problems |= ALARM_WATERING_INEFFECTIVE;
  buzzerAlarm(problems);

  // Demo: neue Bodenfeuchte-Vorgabe unter dem Grenzwert -> sofort einmal gießen,
  // ohne die lange Pause abzuwarten (bewusste Ausnahme für die Vorführung; die
  // harte Mindestpause, Tank, Laufzeit und Tageslimit gelten trotzdem).
  // Nicht, wenn Auto-Gießen abgeschaltet oder wegen Wirkungslosigkeit gesperrt ist.
  if (demoTakeSoilKick() && settings.auto_water && !wateringBlocked && current.soil_ok &&
      current.soil_pct < settings.moisture_min_pct && !pumpRunning()) {
    if (pumpStart(settings.max_pump_s_per_run, "Demo", false)) {
      autoWateredSinceLast = true;
      return;
    }
  }

  autoWater();
}

// Automatische Bewässerung in Stößen: max_pump_s_per_run gießen, pump_cooldown_s
// warten (Wasser versickert, Sensor reagiert), erneut messen, bis zum Ziel.
static void autoWater() {
  if (!settings.auto_water || !current.soil_ok || pumpRunning()) return;
  float soil = current.soil_pct;

  // Gesperrt, weil Gießen nicht wirkte: aufheben, sobald die ECHTE Feuchte wieder
  // steigt (ein Demo-Wert darf die Sperre nicht aufheben)
  if (wateringBlocked) {
    if (!current.demo_soil && soil >= sessionStartSoil + WATER_CHECK_MIN_RISE_PCT)
      clearWateringBlock("Bodenfeuchte steigt wieder");
    else return;
  }

  // Wechsel zwischen Demo- und echtem Bodenwert beendet die Sitzung, damit die
  // Plausibilitätsprüfung nicht mit einem Demo-Startwert rechnet
  if (wateringSession && current.demo_soil != sessionDemo) wateringSession = false;

  // Trockenlaufschutz / Tageslimit: nur melden (höchstens alle 30 s)
  if (tankIsEmpty() || pumpTodaySeconds() >= settings.max_pump_s_per_day) {
    static uint32_t lastBlockMsg = 0;
    if (soil < settings.moisture_min_pct && (lastBlockMsg == 0 || millis() - lastBlockMsg > 30000)) {
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

  // Sitzung starten/beenden
  if (!wateringSession && soil < settings.moisture_min_pct) {
    wateringSession = true;
    sessionStartSoil = soil;
    sessionStartMs = millis();
    sessionRuns = 0;
    sessionDemo = current.demo_soil;
  }
  if (soil >= settings.moisture_target_pct) wateringSession = false;
  if (!wateringSession) return;

  if (pumpCooldownActive()) {
    static uint32_t lastCdMsg = 0;
    if (lastCdMsg == 0 || millis() - lastCdMsg > 30000) {
      lastCdMsg = millis();
      Serial.printf("[PUMPE] Erde trocken, Pause nach letztem Gießen: noch %lu s\n", (unsigned long)pumpCooldownLeftS());
    }
    return;
  }

  // Plausibilität: nach mehreren Läufen (und mind. 3 min) muss die Feuchte gestiegen
  // sein. Im Demo-Modus ist der Wert fest vorgegeben -> Prüfung aussetzen.
  if (!current.demo_soil && sessionRuns >= WATER_CHECK_RUNS && millis() - sessionStartMs >= 180000 &&
      soil < sessionStartSoil + WATER_CHECK_MIN_RISE_PCT) {
    wateringBlocked = true;
    wateringSession = false;
    Serial.printf("[PUMPE] %u Läufe ohne Wirkung (Boden %.1f %% -> %.1f %%): Auto-Gießen gesperrt.\n",
                  (unsigned)sessionRuns, sessionStartSoil, soil);
    return;
  }

  if (pumpStart(settings.max_pump_s_per_run, "auto", true)) {
    autoWateredSinceLast = true;
    if (!current.demo_soil) sessionRuns++;
  } else {
    wateringSession = false;  // Tank leer / Tageslimit -> Sitzung beenden
  }
}

static void clearWateringBlock(const char *why) {
  if (!wateringBlocked) return;
  wateringBlocked = false;
  wateringSession = false;
  Serial.printf("[PUMPE] Sperre Auto-Gießen aufgehoben (%s)\n", why);
}

static void printStatus() {
  Serial.printf("[MESS] Boden %s%.1f %% (roh %d)%s  Licht %s%.1f %% (roh %d)  ", current.demo_soil ? "[DEMO]" : "",
                current.soil_pct, current.soil_raw, current.soil_ok ? "" : " FEHLER", current.demo_light ? "[DEMO]" : "",
                current.light_pct, current.light_raw);
  if (current.dht_ok)
    Serial.printf("Temp %s%.1f °C  LF %s%.0f %%  ", current.demo_temp ? "[DEMO]" : "", current.temp_c,
                  current.demo_hum ? "[DEMO]" : "", current.hum_pct);
  else
    Serial.print("DHT11 FEHLER  ");
  Serial.printf("Tank %.0f ml (%.0f %%)  Pumpe %s (heute %.0f s)%s  WLAN %s\n", tankRemainingMl(), tankLevelPct(),
                pumpRunning() ? "AN" : "aus", pumpTodaySeconds(), wateringBlocked ? " GESPERRT" : "",
                netConnected() ? "ok" : "--");
}

// ---------------------------------------------------------------------------
// Server
// ---------------------------------------------------------------------------

// Übernimmt "config" aus der Serverantwort. Fehlende Felder behalten ihren Wert,
// alle Werte laufen durch configClampSettings(). Gespeichert wird nur bei Änderung.
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

// Führt "commands" aus der Serverantwort aus (Dashboard-Buttons).
// Auch hier gelten alle Pumpengrenzen – der Server kann nur anfragen.
static void handleCommands(JsonObjectConst cmd) {
  if (cmd.isNull()) return;
  if (cmd["tank_refilled"] | false) {
    tankRefill("Dashboard");
    clearWateringBlock("Tank aufgefüllt");
    buzzerBeep(2);
  }
  float pumpS = cmd["pump_run_s"] | 0.0f;
  if (pumpS > 0) pumpStart(pumpS, "manuell (Dashboard)", false);
  if (cmd["buzzer"] | false) buzzerBeep(3);
  if (cmd["identify"] | false) {
    displayIdentify();
    buzzerBeep(1);
  }
}

// Schreibt eine Zahl mit 1 Nachkommastelle, oder null, wenn ungültig.
// (serialized() schreibt den Text roh ins JSON – "nan" wäre ungültiges JSON.)
static void putNumber(JsonDocument &doc, const char *key, float value, bool valid) {
  if (valid && isfinite(value)) doc[key] = serialized(String(value, 1));
  else doc[key] = nullptr;
}

// Baut die Nachricht nach API-Vertrag Abschnitt 2 und sendet sie.
static void sendReading() {
  JsonDocument doc;
  doc["device_id"] = CFG_DEVICE_ID;
  doc["fw_version"] = FW_VERSION;
  doc["seq"] = seq;
  doc["uptime_s"] = millis() / 1000;
  doc["rssi_dbm"] = WiFi.RSSI();

  putNumber(doc, "soil_moisture_pct", current.soil_pct, current.soil_ok);
  doc["soil_moisture_raw"] = current.soil_raw;  // Rohwert immer echt – hilft bei der Fehlersuche
  putNumber(doc, "light_pct", current.light_pct, current.light_ok);
  if (current.light_ok) doc["light_raw"] = current.light_raw;
  else doc["light_raw"] = nullptr;
  putNumber(doc, "air_temp_c", current.temp_c, current.dht_ok);
  putNumber(doc, "air_humidity_pct", current.hum_pct, current.dht_ok);
  putNumber(doc, "tank_remaining_ml", tankRemainingMl(), true);
  putNumber(doc, "water_level_pct", tankLevelPct(), true);
  float pumpS = pumpTakeOnSecondsSinceLast();
  putNumber(doc, "pump_on_s_since_last", pumpS, true);
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
  seq++;
  if (code == 200) {
    autoWateredSinceLast = false;
    applyConfig(resp["config"].as<JsonObjectConst>());
    handleCommands(resp["commands"].as<JsonObjectConst>());
    demoApplyFromServer(resp["demo"]);
    Serial.printf("[HTTP] #%lu gesendet -> 200\n", (unsigned long)(seq - 1));
  } else {
    pumpRestoreOnSeconds(pumpS);  // Laufzeit geht nicht verloren, kommt mit der nächsten Nachricht
    netMarkError();
  }
}

// ---------------------------------------------------------------------------
// Serielle Konsole (nur mit USB-Kabel erreichbar = physischer Zugang)
// ---------------------------------------------------------------------------
static void printHelp() {
  Serial.println(F(
      "Befehle:\n"
      "  status                 aktuelle Werte + Einstellungen\n"
      "  demo soil|light|temp|hum|tank <wert> [sekunden]   Wert überschreiben (Standard 120 s, max 600 s)\n"
      "  demo error dht|soil|light [sekunden]               Sensorausfall simulieren\n"
      "  demo off               Demo beenden\n"
      "  pump <sekunden>        Pumpe manuell (Sicherheitsgrenzen gelten)\n"
      "  stop                   Pumpe sofort aus\n"
      "  refill                 Tank als aufgefüllt markieren (hebt auch die Gieß-Sperre auf)\n"
      "  cal soil dry|wet       aktuellen Rohwert als trocken/nass speichern\n"
      "  cal light dark|bright  aktuellen Rohwert als dunkel/hell speichern\n"
      "  ledtest | ledflip      LED-Bar testen / Richtung umdrehen (bis Neustart)\n"
      "  ledseg <0-10>          genau so viele Segmente 10 s lang anzeigen (Test)\n"
      "  ledswap                Daten-/Taktpin der LED-Bar tauschen (wenn sie nicht reagiert)\n"
      "  mute                   Summer an/aus (bis Neustart)\n"
      "  beep                   Summer testen\n"
      "  send                   sofort an Server senden"));
}

// Kalibrierung setzen und prüfen; ungültige Paare werden nicht gespeichert
static void calibrateSoil(const String &which) {
  int raw = sensorsReadSoilRaw();
  if (which == "dry") calib.soil_raw_dry = raw;
  else if (which == "wet") calib.soil_raw_wet = raw;
  else {
    Serial.println("[CAL] cal soil dry|wet");
    return;
  }
  Serial.printf("[CAL] Boden %s = %d\n", which.c_str(), raw);
  // Nur ein gültiges Paar wird gespeichert. Ein ungültiges bleibt bis zum
  // Neustart im RAM (Bodensensor meldet dann Fehler, es wird nicht gegossen),
  // damit der zweite Kalibrierschritt noch möglich ist.
  if (configSoilCalibrationValid()) {
    configSaveCalibration();
  } else {
    Serial.printf("[CAL] trocken (%d) muss mind. %d über nass (%d) liegen – noch nicht gespeichert.\n",
                  calib.soil_raw_dry, MIN_SOIL_CAL_SPAN, calib.soil_raw_wet);
    Serial.println("[CAL] Trocken: Sensor an der Luft. Nass: bis zur Linie ins Wasser. Dann den anderen Wert messen.");
  }
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
    measureAndAct();  // sofort anwenden (LED-Bar, Demo-Gießen)
  }
  else if (c == "pump" && n >= 2) pumpStart(parts[1].toFloat(), "manuell (seriell)", false);
  else if (c == "stop") pumpStop();
  else if (c == "refill") {
    tankRefill("seriell");
    clearWateringBlock("refill");
    buzzerBeep(2);
  }
  else if (c == "cal" && n >= 3) {
    if (parts[1] == "soil") calibrateSoil(parts[2]);
    else if (parts[1] == "light") {
      int raw = sensorsReadLightRaw();
      if (parts[2] == "dark") calib.light_raw_dark = raw;
      else if (parts[2] == "bright") calib.light_raw_bright = raw;
      Serial.printf("[CAL] Licht %s = %d\n", parts[2].c_str(), raw);
      // Speichert die ganze Struktur -> nur, wenn auch die Bodenwerte gültig sind
      if (configSoilCalibrationValid()) configSaveCalibration();
      else Serial.println("[CAL] erst Boden gültig kalibrieren, dann wird gespeichert");
    }
  }
  else if (c == "ledtest") displayLedTest();
  else if (c == "ledflip") displayFlip();
  else if (c == "ledseg" && n >= 2) displayTestSegments(parts[1].toInt());
  else if (c == "ledswap") displaySwapPins();
  else if (c == "mute") {
    CFG_BUZZER_ENABLED = !CFG_BUZZER_ENABLED;
    Serial.printf("[SUMMER] %s\n", CFG_BUZZER_ENABLED ? "an" : "stumm");
  }
  else if (c == "beep") buzzerBeepForced(2);
  else if (c == "send") lastSendMs = millis() - settings.interval_s * 1000UL;
  else Serial.println("Unbekannter Befehl – \"help\" eingeben");
}

static void pollSerial() {
  while (Serial.available()) {
    char ch = (char)Serial.read();
    if (ch == '\n' || ch == '\r') {
      if (serialLine.length()) handleSerialCommand(serialLine);
      serialLine = "";
    } else if (serialLine.length() < 80) {  // Zeilenlänge begrenzen
      serialLine += ch;
    }
  }
}

// BOOT-Taste 3 s halten = Tank aufgefüllt (funktioniert auch ohne WLAN/PC)
static void pollBootButton() {
  if (digitalRead(PIN_BOOT_BTN) == LOW) {
    if (bootPressedSince == 0) bootPressedSince = millis();
    if (!bootHandled && millis() - bootPressedSince > 3000) {
      bootHandled = true;
      tankRefill("BOOT-Taste");
      clearWateringBlock("Tank aufgefüllt");
      buzzerBeep(2);
    }
  } else {
    bootPressedSince = 0;
    bootHandled = false;
  }
}

// Task-Watchdog: wird loop() länger als WATCHDOG_TIMEOUT_S nicht durchlaufen,
// startet der ESP neu. pumpBegin() schaltet dann als Erstes das Relais aus.
static void watchdogBegin() {
  esp_task_wdt_config_t cfg = {};
  cfg.timeout_ms = WATCHDOG_TIMEOUT_S * 1000;
  cfg.idle_core_mask = 1 << 0;  // wie IDF-Standard: Idle-Task von Core 0 (WLAN) mit überwachen
  cfg.trigger_panic = true;     // bei Timeout Neustart
  esp_err_t e = esp_task_wdt_reconfigure(&cfg);
  if (e != ESP_OK) e = esp_task_wdt_init(&cfg);
  if (e == ESP_OK) e = esp_task_wdt_add(NULL);  // aktuelle Task = loop()
  if (e == ESP_OK) Serial.printf("[WDT] Watchdog aktiv (%lu s)\n", (unsigned long)WATCHDOG_TIMEOUT_S);
  else Serial.printf("[WDT] Watchdog konnte nicht aktiviert werden (Fehler %d)\n", (int)e);
}

// Meldet, warum der ESP zuletzt neu gestartet ist (Watchdog/Absturz = Hinweis auf Fehler)
static void printResetReason() {
  esp_reset_reason_t r = esp_reset_reason();
  const char *txt = "Einschalten/Reset";
  if (r == ESP_RST_TASK_WDT || r == ESP_RST_INT_WDT || r == ESP_RST_WDT) txt = "WATCHDOG (Firmware hing)";
  else if (r == ESP_RST_PANIC) txt = "ABSTURZ";
  else if (r == ESP_RST_BROWNOUT) txt = "Unterspannung (Netzteil/Kabel prüfen)";
  else if (r == ESP_RST_SW) txt = "Software-Neustart";
  Serial.printf("[BOOT] Grund des Neustarts: %s\n", txt);
}

// ---------------------------------------------------------------------------
void setup() {
  pumpBegin();  // ZUERST: Relais sicher AUS, egal was danach passiert
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.printf("=== Smart Garden ESP32 – Firmware %s ===\n", FW_VERSION);
  printResetReason();

  pinMode(PIN_BOOT_BTN, INPUT_PULLUP);
  configBegin();
  Serial.printf("Gerät %s\n", CFG_DEVICE_ID);
  tankBegin();
  sensorsBegin();
  displayBegin();
  configPrint();
  if (configHasPlaceholders())
    Serial.println("[CFG] WARNUNG: WLAN-Passwort/API-Key sind noch Platzhalter -> nur Offline-Betrieb");
  displayLedTest();
  buzzerBeep(1);
  netBegin();
  printHelp();
  if (tankIsEmpty())
    Serial.printf("[TANK] Achtung: Tank laut Schätzung leer (%.0f ml) – Pumpe gesperrt. \"refill\" eingeben, wenn aufgefüllt.\n",
                  tankRemainingMl());
  lastSendMs = millis() - settings.interval_s * 1000UL + 5000;  // erste Nachricht nach ~5 s
  watchdogBegin();
}

void loop() {
  esp_task_wdt_reset();  // "ich lebe noch"
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

  // Nicht senden, während die Pumpe läuft: HTTP kann bis zu 8 s blockieren,
  // die Pumpe würde sonst zu spät ausgeschaltet.
  if (!pumpRunning() && now - lastSendMs >= settings.interval_s * 1000UL) {
    lastSendMs = now;
    printStatus();
    if (netConnected()) sendReading();
    else Serial.println("[HTTP] kein WLAN – Senden übersprungen");
  }
}
