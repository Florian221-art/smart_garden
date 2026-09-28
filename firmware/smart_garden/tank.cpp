#include "tank.h"
#include "garden_config.h"
#include <Preferences.h>

static float remainingMl = -1;
static Preferences prefs;

static void save() {
  prefs.begin("garden", false);
  prefs.putFloat("tank_ml", remainingMl);
  prefs.end();
}

void tankBegin() {
  prefs.begin("garden", true);
  remainingMl = prefs.getFloat("tank_ml", -1);
  prefs.end();
  if (remainingMl < 0) remainingMl = settings.tank_capacity_ml;  // erster Start: voll
  remainingMl = constrain(remainingMl, 0.0f, settings.tank_capacity_ml);
}

void tankConsume(float pump_seconds) {
  remainingMl = max(0.0f, remainingMl - pump_seconds * settings.pump_flow_ml_per_s);
  save();
}

void tankRefill() {
  remainingMl = settings.tank_capacity_ml;
  save();
  Serial.printf("[TANK] aufgefüllt -> %.0f ml\n", remainingMl);
}

void tankSetPct(float pct) {
  remainingMl = settings.tank_capacity_ml * constrain(pct, 0.0f, 100.0f) / 100.0f;
  save();
}

float tankRemainingMl() { return min(remainingMl, settings.tank_capacity_ml); }

float tankLevelPct() {
  if (settings.tank_capacity_ml <= 0) return 0;
  return constrain(tankRemainingMl() * 100.0f / settings.tank_capacity_ml, 0.0f, 100.0f);
}

bool tankIsEmpty() { return tankLevelPct() <= TANK_EMPTY_PCT; }
bool tankIsLow() { return tankLevelPct() <= settings.tank_low_pct; }
