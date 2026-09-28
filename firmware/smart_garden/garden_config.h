// garden_config.h – Pins, Standardwerte, Einstellungen (Server-Config + Kalibrierung)
#pragma once
#include <Arduino.h>

#define FW_VERSION "0.2.3"

// ---------------------------------------------------------------------------
// Zugangsdaten (WLAN, Server, API-Key) – werden OBEN in smart_garden.ino eingetragen
// ---------------------------------------------------------------------------
extern const char *CFG_WIFI_SSID;
extern const char *CFG_WIFI_PASSWORD;
extern const char *CFG_SERVER_URL;
extern const char *CFG_API_KEY;
extern const char *CFG_DEVICE_ID;
extern bool CFG_LEDBAR_REVERSE;
extern bool CFG_LEDBAR_SWAP_PINS;
extern int CFG_LEDBAR_MODE;
extern bool CFG_BUZZER_ENABLED;

// ---------------------------------------------------------------------------
// Pins (siehe docs/hardware/verkabelung.md)
// ---------------------------------------------------------------------------
constexpr uint8_t PIN_SOIL = 34;      // Kapazitiver Bodenfeuchtesensor AOUT (ADC1)
constexpr uint8_t PIN_LIGHT = 35;     // LDR-Modul AO (ADC1)
constexpr uint8_t PIN_DHT = 4;        // DHT11 SIG
constexpr uint8_t PIN_LEDBAR_DCKI = 18; // Grove LED Bar DCKI (Takt)  – gelbes Grove-Kabel (im Test ermittelt)
constexpr uint8_t PIN_LEDBAR_DI = 19;   // Grove LED Bar DI (Daten)   – weißes Grove-Kabel
constexpr uint8_t PIN_BUZZER = 26;    // aktiver Summer (+)
constexpr uint8_t PIN_RELAY = 27;     // Grove Relay SIG (HIGH = Pumpe an)
constexpr uint8_t PIN_BOOT_BTN = 0;   // BOOT-Taste (3 s halten = Tank aufgefüllt)
constexpr uint8_t PIN_STATUS_LED = 2; // Onboard-LED

// ---------------------------------------------------------------------------
// Harte Sicherheitsgrenzen – kann der Server NICHT überschreiben
// ---------------------------------------------------------------------------
constexpr float HARD_MAX_PUMP_S_PER_RUN = 15.0f;
constexpr float HARD_MAX_PUMP_S_PER_DAY = 300.0f;
constexpr uint32_t HARD_MIN_COOLDOWN_S = 10;
constexpr float TANK_EMPTY_PCT = 5.0f;      // darunter keine Bewässerung (Trockenlaufschutz)
constexpr uint32_t MAX_DEMO_S = 600;

// Zeiten
constexpr uint32_t SENSOR_PERIOD_MS = 2000;  // lokale Messung (LED-Bar, Auto-Bewässerung)
constexpr uint32_t HTTP_TIMEOUT_MS = 4000;
constexpr uint32_t WIFI_RETRY_MS = 10000;
constexpr uint32_t ALARM_REPEAT_MS = 600000; // Summer wiederholt einen bestehenden Alarm höchstens alle 10 min

// ---------------------------------------------------------------------------
// Einstellungen vom Server (API-Vertrag "config") – werden im Flash gespeichert
// ---------------------------------------------------------------------------
struct Settings {
  uint32_t interval_s = 15;
  float moisture_min_pct = 30;
  float moisture_target_pct = 55;
  bool auto_water = true;
  float max_pump_s_per_run = 5;
  uint32_t pump_cooldown_s = 300;
  float max_pump_s_per_day = 60;
  bool buzzer_enabled = true;
  float tank_capacity_ml = 1500;
  float pump_flow_ml_per_s = 20;
  float tank_low_pct = 20;
};

// Kalibrierung der Analogsensoren (seriell: "cal soil dry", "cal soil wet", ...)
struct Calibration {
  int soil_raw_dry = 2900;   // Sensor an der Luft / trockene Erde
  int soil_raw_wet = 1300;   // Sensor in Wasser (bis zur Linie!)
  int light_raw_dark = 4095; // abgedeckt
  int light_raw_bright = 300; // Handylampe direkt drauf
  bool ledbar_green_to_red = false;  // ungenutzt (Richtung jetzt über CFG_LEDBAR_REVERSE)
};

extern Settings settings;
extern Calibration calib;

void configBegin();           // lädt Settings + Kalibrierung aus NVS
void configSaveSettings();
void configSaveCalibration();
void configClampSettings(Settings &s); // harte Grenzen anwenden
void configPrint();
