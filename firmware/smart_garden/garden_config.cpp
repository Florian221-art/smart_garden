#include "garden_config.h"
#include <Preferences.h>

Settings settings;
Calibration calib;

static Preferences prefs;

void configClampSettings(Settings &s) {
  s.interval_s = constrain(s.interval_s, (uint32_t)5, (uint32_t)600);
  s.moisture_min_pct = constrain(s.moisture_min_pct, 0.0f, 100.0f);
  s.moisture_target_pct = constrain(s.moisture_target_pct, s.moisture_min_pct, 100.0f);
  s.max_pump_s_per_run = constrain(s.max_pump_s_per_run, 0.5f, HARD_MAX_PUMP_S_PER_RUN);
  s.pump_cooldown_s = max(s.pump_cooldown_s, HARD_MIN_COOLDOWN_S);
  s.max_pump_s_per_day = constrain(s.max_pump_s_per_day, 0.0f, HARD_MAX_PUMP_S_PER_DAY);
  s.tank_capacity_ml = constrain(s.tank_capacity_ml, 50.0f, 50000.0f);
  s.pump_flow_ml_per_s = constrain(s.pump_flow_ml_per_s, 0.5f, 500.0f);
  s.tank_low_pct = constrain(s.tank_low_pct, TANK_EMPTY_PCT, 100.0f);
}

void configBegin() {
  prefs.begin("garden", true);
  if (prefs.isKey("settings") && prefs.getBytesLength("settings") == sizeof(Settings)) {
    prefs.getBytes("settings", &settings, sizeof(Settings));
  }
  if (prefs.isKey("calib") && prefs.getBytesLength("calib") == sizeof(Calibration)) {
    prefs.getBytes("calib", &calib, sizeof(Calibration));
  }
  prefs.end();
  configClampSettings(settings);
}

void configSaveSettings() {
  prefs.begin("garden", false);
  prefs.putBytes("settings", &settings, sizeof(Settings));
  prefs.end();
}

void configSaveCalibration() {
  prefs.begin("garden", false);
  prefs.putBytes("calib", &calib, sizeof(Calibration));
  prefs.end();
}

void configPrint() {
  Serial.printf("[CFG] interval=%lus min=%.0f%% ziel=%.0f%% auto=%d pumpe max=%.1fs cooldown=%lus tag=%.0fs\n",
                (unsigned long)settings.interval_s, settings.moisture_min_pct, settings.moisture_target_pct, settings.auto_water,
                settings.max_pump_s_per_run, (unsigned long)settings.pump_cooldown_s, settings.max_pump_s_per_day);
  Serial.printf("[CFG] tank=%.0fml durchfluss=%.1fml/s tank_low=%.0f%% summer=%d\n", settings.tank_capacity_ml,
                settings.pump_flow_ml_per_s, settings.tank_low_pct, settings.buzzer_enabled);
  Serial.printf("[CAL] boden trocken=%d nass=%d | licht dunkel=%d hell=%d | ledbar greenToRed=%d\n", calib.soil_raw_dry,
                calib.soil_raw_wet, calib.light_raw_dark, calib.light_raw_bright, calib.ledbar_green_to_red);
}
