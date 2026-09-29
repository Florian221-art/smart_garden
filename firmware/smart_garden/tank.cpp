// =============================================================================
//  tank.cpp – Tank-Schätzung (siehe tank.h)
// =============================================================================
#include "tank.h"
#include "garden_config.h"
#include <Preferences.h>

static float remainingMl = -1;  // echte Schätzung in ml (-1 = noch nie gespeichert)
static float demoPct = -1;      // Demo-Füllstand in %, -1 = keine Demo
static Preferences prefs;

// Schreibt die Restmenge in den Flash. Passiert nur nach einem Pumpenlauf oder
// beim Auffüllen (wenige Male am Tag) – unkritisch für die Flash-Lebensdauer.
static void save() {
  prefs.begin("garden", false);
  prefs.putFloat("tank_ml", remainingMl);
  prefs.end();
}

// echter geschätzter Füllstand in %, unabhängig vom Demo-Modus
static float realPct() {
  if (settings.tank_capacity_ml <= 0) return 0;
  float ml = min(remainingMl, settings.tank_capacity_ml);
  return constrain(ml * 100.0f / settings.tank_capacity_ml, 0.0f, 100.0f);
}

void tankBegin() {
  prefs.begin("garden", true);
  remainingMl = prefs.getFloat("tank_ml", -1);
  prefs.end();
  if (isnan(remainingMl) || remainingMl < 0) remainingMl = settings.tank_capacity_ml;  // erster Start: voll
  remainingMl = constrain(remainingMl, 0.0f, settings.tank_capacity_ml);
}

void tankConsume(float pump_seconds) {
  if (!(pump_seconds > 0)) return;  // fängt auch NaN ab
  remainingMl = max(0.0f, remainingMl - pump_seconds * settings.pump_flow_ml_per_s);
  save();
}

void tankRefill(const char *source) {
  remainingMl = settings.tank_capacity_ml;
  save();
  Serial.printf("[TANK] als aufgefüllt markiert (%s) -> %.0f ml\n", source, remainingMl);
}

void tankSetDemoPct(float pct) {
  demoPct = (pct >= 0 && !isnan(pct)) ? constrain(pct, 0.0f, 100.0f) : -1.0f;
}

float tankLevelPct() { return demoPct >= 0 ? demoPct : realPct(); }

float tankRemainingMl() { return settings.tank_capacity_ml * tankLevelPct() / 100.0f; }

bool tankIsEmpty() {
  float pct = realPct();
  if (demoPct >= 0) pct = min(pct, demoPct);  // Demo darf nur "leerer" machen, nie "voller"
  return pct <= TANK_EMPTY_PCT;
}

bool tankIsLow() { return tankLevelPct() <= settings.tank_low_pct; }
