// =============================================================================
//  garden_config.cpp – Einstellungen und Kalibrierung laden/speichern/prüfen
// =============================================================================
#include "garden_config.h"
#include <Preferences.h>

// Optionale Datei mit Zugangsdaten (von Git ignoriert). Ist sie vorhanden,
// ersetzen ihre Werte die Platzhalter oben im Sketch – so muss das echte
// WLAN-Passwort nie im Sketch stehen. Vorlage: secrets.h.example
#if __has_include("secrets.h")
#include "secrets.h"
#define HAVE_SECRETS_H 1
#endif

Settings settings;
Calibration calib;

static Preferences prefs;

// Begrenzt jeden Wert auf einen sicheren Bereich. Wird beim Laden aus dem Flash
// UND auf jede Server-Config angewendet – ein fehlerhafter oder manipulierter
// Server kann die harten Grenzen aus garden_config.h nicht aushebeln.
void configClampSettings(Settings &s) {
  s.interval_s = constrain(s.interval_s, (uint32_t)5, (uint32_t)600);
  s.moisture_min_pct = constrain(s.moisture_min_pct, 0.0f, 100.0f);
  s.moisture_target_pct = constrain(s.moisture_target_pct, s.moisture_min_pct, 100.0f);
  s.max_pump_s_per_run = constrain(s.max_pump_s_per_run, 0.5f, HARD_MAX_PUMP_S_PER_RUN);
  s.pump_cooldown_s = constrain(s.pump_cooldown_s, HARD_MIN_COOLDOWN_S, HARD_MAX_COOLDOWN_S);
  s.max_pump_s_per_day = constrain(s.max_pump_s_per_day, 0.0f, HARD_MAX_PUMP_S_PER_DAY);
  s.tank_capacity_ml = constrain(s.tank_capacity_ml, 50.0f, 50000.0f);
  s.pump_flow_ml_per_s = constrain(s.pump_flow_ml_per_s, 0.5f, 500.0f);
  s.tank_low_pct = constrain(s.tank_low_pct, TANK_EMPTY_PCT, 100.0f);
  // NaN (kann aus einem beschädigten Flash-Blob stammen) auf Standard zurücksetzen
  Settings d;
  if (isnan(s.moisture_min_pct)) s.moisture_min_pct = d.moisture_min_pct;
  if (isnan(s.moisture_target_pct)) s.moisture_target_pct = d.moisture_target_pct;
  if (isnan(s.max_pump_s_per_run)) s.max_pump_s_per_run = d.max_pump_s_per_run;
  if (isnan(s.max_pump_s_per_day)) s.max_pump_s_per_day = d.max_pump_s_per_day;
  if (isnan(s.tank_capacity_ml)) s.tank_capacity_ml = d.tank_capacity_ml;
  if (isnan(s.pump_flow_ml_per_s)) s.pump_flow_ml_per_s = d.pump_flow_ml_per_s;
  if (isnan(s.tank_low_pct)) s.tank_low_pct = d.tank_low_pct;
}

bool configSoilCalibrationValid() {
  // Kapazitiver Sensor: trocken = hoher Rohwert. Liegen die Werte zu dicht
  // beieinander (z. B. zweimal "cal soil dry"), wären die Prozente Unsinn –
  // im schlimmsten Fall dauerhaft 0 % und damit Dauergießen.
  return calib.soil_raw_dry - calib.soil_raw_wet >= MIN_SOIL_CAL_SPAN;
}

bool configHasPlaceholders() {
  return strcmp(CFG_WIFI_PASSWORD, PLACEHOLDER_WIFI_PASSWORD) == 0 || strcmp(CFG_API_KEY, PLACEHOLDER_API_KEY) == 0;
}

void configBegin() {
#ifdef HAVE_SECRETS_H
  // Werte aus secrets.h haben Vorrang vor dem Sketch (jeweils nur, wenn definiert)
#ifdef SECRET_WIFI_SSID
  CFG_WIFI_SSID = SECRET_WIFI_SSID;
#endif
#ifdef SECRET_WIFI_PASSWORD
  CFG_WIFI_PASSWORD = SECRET_WIFI_PASSWORD;
#endif
#ifdef SECRET_SERVER_URL
  CFG_SERVER_URL = SECRET_SERVER_URL;
#endif
#ifdef SECRET_API_KEY
  CFG_API_KEY = SECRET_API_KEY;
#endif
#ifdef SECRET_DEVICE_ID
  CFG_DEVICE_ID = SECRET_DEVICE_ID;
#endif
  Serial.println("[CFG] Zugangsdaten aus secrets.h übernommen");
#endif

  // Nur übernehmen, wenn die gespeicherte Größe exakt zur aktuellen Struktur passt
  prefs.begin("garden", true);
  // Gespeicherte Settings nur übernehmen, wenn Größe UND Settings-Version passen.
  // Sonst gelten die neuen Standardwerte (bis der Server etwas anderes schickt).
  bool resetSettings = false;
  if (prefs.isKey("settings") && prefs.getBytesLength("settings") == sizeof(Settings) &&
      prefs.getUChar("set_ver", 0) == SETTINGS_VERSION) {
    prefs.getBytes("settings", &settings, sizeof(Settings));
  } else if (prefs.isKey("settings")) {
    resetSettings = true;
  }
  if (prefs.isKey("calib") && prefs.getBytesLength("calib") == sizeof(Calibration)) {
    prefs.getBytes("calib", &calib, sizeof(Calibration));
  }
  prefs.end();
  configClampSettings(settings);
  if (resetSettings) {  // einmalig: neue Standardwerte mit aktueller Version speichern
    configSaveSettings();
    Serial.println("[CFG] neue Standard-Einstellungen dieser Firmware übernommen (alte verworfen)");
  }
}

void configSaveSettings() {
  prefs.begin("garden", false);
  prefs.putBytes("settings", &settings, sizeof(Settings));
  prefs.putUChar("set_ver", SETTINGS_VERSION);
  prefs.end();
}

void configSaveCalibration() {
  prefs.begin("garden", false);
  prefs.putBytes("calib", &calib, sizeof(Calibration));
  prefs.end();
}

// Gibt alle Einstellungen aus. Passwort und API-Key werden bewusst NICHT
// ausgegeben (serielle Logs landen gern in Screenshots/Chats).
void configPrint() {
  Serial.printf("[CFG] WLAN \"%s\"  Server %s  Gerät %s  Passwort/Key %s\n", CFG_WIFI_SSID, CFG_SERVER_URL,
                CFG_DEVICE_ID, configHasPlaceholders() ? "NOCH PLATZHALTER" : "gesetzt");
  Serial.printf("[CFG] interval=%lus min=%.0f%% ziel=%.0f%% auto=%d pumpe max=%.1fs cooldown=%lus tag=%.0fs\n",
                (unsigned long)settings.interval_s, settings.moisture_min_pct, settings.moisture_target_pct, settings.auto_water,
                settings.max_pump_s_per_run, (unsigned long)settings.pump_cooldown_s, settings.max_pump_s_per_day);
  Serial.printf("[CFG] tank=%.0fml durchfluss=%.1fml/s tank_low=%.0f%% summer=%d\n", settings.tank_capacity_ml,
                settings.pump_flow_ml_per_s, settings.tank_low_pct, settings.buzzer_enabled);
  Serial.printf("[CAL] boden trocken=%d nass=%d%s | licht dunkel=%d hell=%d\n", calib.soil_raw_dry, calib.soil_raw_wet,
                configSoilCalibrationValid() ? "" : " (UNGUELTIG -> neu kalibrieren)", calib.light_raw_dark,
                calib.light_raw_bright);
  Serial.printf("[LED] modus=%d (0=Zeiger, 1=Balken ab Rot, 2=Trockenheit ab Gruen) reverse=%d swap=%d | Summer %s\n",
                CFG_LEDBAR_MODE, CFG_LEDBAR_REVERSE, CFG_LEDBAR_SWAP_PINS,
                (CFG_BUZZER_ENABLED && settings.buzzer_enabled) ? "an" : "stumm");
}
