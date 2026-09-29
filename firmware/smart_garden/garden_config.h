// =============================================================================
//  garden_config.h – zentrale Konstanten und Einstellungen der Firmware
// =============================================================================
//  Inhalt:
//    1. Zugangsdaten (werden OBEN in smart_garden.ino eingetragen, hier nur deklariert)
//    2. Pinbelegung (muss zu docs/hardware/verkabelung.md passen)
//    3. Harte Sicherheitsgrenzen – fest einkompiliert, vom Server NICHT änderbar
//    4. Zeiten, Alarm-Bits
//    5. Settings    – Einstellungen, die der Server per "config" schicken darf
//    6. Calibration – Kalibrierwerte der Analogsensoren (seriell gesetzt)
//
//  Settings und Calibration liegen im NVS-Flash (Preferences, Namespace "garden")
//  und überleben Neustarts und neues Hochladen der Firmware.
//  Ändert sich die Größe einer Struktur (Feld hinzugefügt/entfernt), erkennt
//  configBegin() das an der Blob-Länge und startet mit den Standardwerten.
// =============================================================================
#pragma once
#include <Arduino.h>

#define FW_VERSION "0.3.5"  // bei jeder Änderung erhöhen UND ganz oben in smart_garden.ino eintragen

// -----------------------------------------------------------------------------
// 1. Zugangsdaten + Schalter – definiert ganz oben in smart_garden.ino.
//    Optional können WLAN-Passwort, API-Key usw. in einer Datei secrets.h im
//    Sketch-Ordner stehen (von Git ignoriert), siehe secrets.h.example.
//    Die Zeiger sind absichtlich veränderbar, damit configBegin() sie durch die
//    Werte aus secrets.h ersetzen kann.
// -----------------------------------------------------------------------------
extern const char *CFG_WIFI_SSID;
extern const char *CFG_WIFI_PASSWORD;
extern const char *CFG_SERVER_URL;
extern const char *CFG_API_KEY;
extern const char *CFG_DEVICE_ID;
extern bool CFG_LEDBAR_REVERSE;
extern bool CFG_LEDBAR_SWAP_PINS;
extern int CFG_LEDBAR_BRIGHTNESS;
extern bool CFG_LEDBAR_SLOW;
extern bool CFG_LEDBAR_LATCH_CLOCK;
extern int CFG_LEDBAR_MODE;
extern bool CFG_BUZZER_ENABLED;

// Platzhalter aus dem Sketch – solange diese drinstehen, warnt die Firmware beim Start.
#define PLACEHOLDER_WIFI_PASSWORD "hier-passwort"
#define PLACEHOLDER_API_KEY "hier-api-key"

// -----------------------------------------------------------------------------
// 2. Pins (siehe docs/hardware/verkabelung.md)
//    - Analoge Sensoren nur an ADC1 (GPIO32-39): ADC2 ist bei aktivem WLAN belegt.
//    - Strapping-Pins (0, 2, 12, 15) nicht für externe Bauteile verwenden.
//    - GPIO27 ist nach dem Reset LOW -> die Pumpe läuft beim Einschalten nicht an.
// -----------------------------------------------------------------------------
constexpr uint8_t PIN_SOIL = 34;        // Kapazitiver Bodenfeuchtesensor AOUT (ADC1, nur Eingang)
constexpr uint8_t PIN_LIGHT = 35;       // LDR-Modul AO (ADC1, nur Eingang)
constexpr uint8_t PIN_DHT = 4;          // DHT11 SIG (Pull-up sitzt auf dem Grove-Modul)
// Grove-Stecker der LED-Bar (Aufdruck auf der Platine: DI, DCKI, VCC, GND):
// gelbes Kabel = DI (Daten), weißes Kabel = DCKI (Takt). Am 29.09. am Board bestätigt.
constexpr uint8_t PIN_LEDBAR_DI = 18;   // Grove LED Bar DI (Daten) – gelbes Grove-Kabel an D18
constexpr uint8_t PIN_LEDBAR_DCKI = 19; // Grove LED Bar DCKI (Takt) – weißes Grove-Kabel an D19
constexpr uint8_t PIN_BUZZER = 26;      // aktiver Summer (+), HIGH = Ton
constexpr uint8_t PIN_RELAY = 27;       // Grove Relay SIG, HIGH = Pumpe an
constexpr uint8_t PIN_BOOT_BTN = 0;     // BOOT-Taste onboard (3 s halten = Tank aufgefüllt)
constexpr uint8_t PIN_STATUS_LED = 2;   // Onboard-LED (WLAN-Status)

// -----------------------------------------------------------------------------
// 3. Harte Sicherheitsgrenzen – kann der Server NICHT überschreiben.
//    Sie begrenzen den Schaden, falls Server, Netzwerk oder Einstellungen
//    fehlerhaft oder manipuliert sind (siehe docs/hardware/sicherheit-esp.md).
// -----------------------------------------------------------------------------
constexpr float HARD_MAX_PUMP_S_PER_RUN = 15.0f;  // längster einzelner Pumpenlauf
constexpr float HARD_MAX_PUMP_S_PER_DAY = 300.0f; // Obergrenze für das Tageslimit
constexpr uint32_t HARD_MIN_COOLDOWN_S = 10;      // Mindestpause zwischen zwei Läufen – gilt für ALLE Starts
constexpr uint32_t HARD_MAX_COOLDOWN_S = 86400;   // Obergrenze der Pause (verhindert Überlauf bei s * 1000)
constexpr float TANK_EMPTY_PCT = 5.0f;            // darunter keine Bewässerung (Trockenlaufschutz)
constexpr uint32_t MAX_DEMO_S = 600;              // Demo-Werte gelten höchstens 10 min
constexpr int MIN_SOIL_CAL_SPAN = 300;            // Rohwert trocken und nass müssen mind. so weit auseinander liegen

// Plausibilitätsprüfung der Bewässerung: Steigt die Bodenfeuchte in einer
// Gieß-Sitzung nach WATER_CHECK_RUNS Pumpenläufen um weniger als
// WATER_CHECK_MIN_RISE_PCT, wird die Auto-Bewässerung gesperrt (Sensor steckt
// nicht in der Erde, Schlauch liegt daneben, Pumpe saugt Luft ...).
// Aufheben: "refill", BOOT-Taste 3 s, oder die Bodenfeuchte steigt wieder.
constexpr uint8_t WATER_CHECK_RUNS = 3;
constexpr float WATER_CHECK_MIN_RISE_PCT = 3.0f;

// -----------------------------------------------------------------------------
// 4. Zeiten und Alarm-Bits
// -----------------------------------------------------------------------------
constexpr uint32_t SENSOR_PERIOD_MS = 2000;   // lokale Messung (LED-Bar, Auto-Bewässerung)
constexpr uint32_t HTTP_TIMEOUT_MS = 4000;    // Verbindungs- und Antwort-Timeout
constexpr uint32_t WIFI_RETRY_MS = 10000;     // neuer Verbindungsversuch, falls WLAN weg
constexpr uint32_t ALARM_REPEAT_MS = 600000;  // Summer wiederholt einen bestehenden Alarm höchstens alle 10 min
constexpr uint32_t WATCHDOG_TIMEOUT_S = 20;   // hängt loop() länger, startet der ESP neu (Relais -> AUS)

// Bitmaske für buzzerAlarm()
constexpr uint8_t ALARM_TANK_EMPTY = 1;
constexpr uint8_t ALARM_SOIL_SENSOR = 2;
constexpr uint8_t ALARM_DHT = 4;
constexpr uint8_t ALARM_WATERING_INEFFECTIVE = 8;

// -----------------------------------------------------------------------------
// 5. Einstellungen vom Server (API-Vertrag "config") – im Flash gespeichert.
//    Die Standardwerte gelten, bis der Server etwas anderes schickt.
//    configClampSettings() begrenzt alle Werte auf sichere Bereiche.
// -----------------------------------------------------------------------------
struct Settings {
  uint32_t interval_s = 10;         // Sende-/Anzeigeintervall in s (Server-Config hat Vorrang)
  float moisture_min_pct = 30;      // darunter startet eine Gieß-Sitzung
  float moisture_target_pct = 55;   // bis hierhin wird gegossen
  bool auto_water = true;           // automatische Bewässerung an/aus
  float max_pump_s_per_run = 5;     // Länge eines Pumpenstoßes
  uint32_t pump_cooldown_s = 300;   // Pause zwischen zwei Stößen (Wasser versickern lassen)
  float max_pump_s_per_day = 60;    // Tageslimit (höchstens HARD_MAX_PUMP_S_PER_DAY)
  bool buzzer_enabled = true;       // Summer bei Alarmen
  float tank_capacity_ml = 1500;    // Tankgröße
  float pump_flow_ml_per_s = 20;    // Förderleistung der Pumpe (einmal messen!)
  float tank_low_pct = 20;          // ab hier "Tank bald leer" im Dashboard
};

// -----------------------------------------------------------------------------
// 6. Kalibrierung der Analogsensoren (seriell: "cal soil dry", "cal soil wet", ...)
// -----------------------------------------------------------------------------
struct Calibration {
  int soil_raw_dry = 2900;            // Sensor an der Luft / trockene Erde (kapazitiv: hoher Wert)
  int soil_raw_wet = 1300;            // Sensor bis zur Linie in Wasser (niedriger Wert)
  int light_raw_dark = 4095;          // LDR abgedeckt
  int light_raw_bright = 300;         // Handylampe direkt drauf
  bool ledbar_green_to_red = false;   // ungenutzt – bleibt nur, damit gespeicherte Kalibrierungen gültig bleiben
};

extern Settings settings;
extern Calibration calib;

void configBegin();                     // lädt Settings + Kalibrierung aus NVS, übernimmt secrets.h
void configSaveSettings();              // Settings in NVS schreiben
void configSaveCalibration();           // Kalibrierung in NVS schreiben
void configClampSettings(Settings &s);  // harte Grenzen anwenden (auch auf Serverwerte)
bool configSoilCalibrationValid();      // liegen trocken/nass weit genug auseinander?
bool configHasPlaceholders();           // stehen noch Platzhalter statt Passwort/Key drin?
void configPrint();                     // alles seriell ausgeben (ohne Passwort/Key)
