#include "demo.h"
#include "garden_config.h"
#include "tank.h"

enum Field { F_SOIL, F_LIGHT, F_TEMP, F_HUM, F_TANK, F_COUNT };
static const char *FIELD_NAMES[F_COUNT] = {"soil_moisture_pct", "light_pct", "air_temp_c", "air_humidity_pct",
                                           "water_level_pct"};

static bool has[F_COUNT] = {false};
static float val[F_COUNT] = {0};
static bool errDht = false, errSoil = false, errLight = false;
static uint32_t untilMs = 0;
static bool active = false;
static bool soilKick = false;  // neue Bodenfeuchte-Vorgabe -> sofort einmal gießen (Demo)

bool demoTakeSoilKick() {
  bool k = soilKick;
  soilKick = false;
  return k;
}

static int fieldIndex(const String &name) {
  for (int i = 0; i < F_COUNT; i++)
    if (name == FIELD_NAMES[i]) return i;
  // Kurzformen für die serielle Konsole
  if (name == "soil") return F_SOIL;
  if (name == "light") return F_LIGHT;
  if (name == "temp") return F_TEMP;
  if (name == "hum") return F_HUM;
  if (name == "tank") return F_TANK;
  return -1;
}

static void clearAll() {
  for (int i = 0; i < F_COUNT; i++) has[i] = false;
  errDht = errSoil = errLight = false;
  active = false;
  untilMs = 0;
}

static void startTimer(uint32_t seconds) {
  seconds = min(seconds, MAX_DEMO_S);
  untilMs = millis() + seconds * 1000UL;
  active = true;
}

void demoOff() {
  if (active) Serial.println("[DEMO] aus – wieder echte Messwerte");
  clearAll();
}

bool demoActive() {
  if (active && (int32_t)(millis() - untilMs) >= 0) {
    Serial.println("[DEMO] abgelaufen");
    clearAll();
  }
  return active;
}

static void applyTankIfNeeded(bool changed) {
  if (has[F_TANK] && changed) tankSetPct(val[F_TANK]);
}

void demoApplyFromServer(JsonVariantConst demo) {
  if (demo.isNull()) {
    if (active) demoOff();
    return;
  }
  uint32_t ttl = demo["expires_in_s"] | 120;
  bool changed = false;
  bool newHas[F_COUNT] = {false};
  float newVal[F_COUNT] = {0};
  JsonObjectConst ov = demo["overrides"];
  for (JsonPairConst kv : ov) {
    int i = fieldIndex(String(kv.key().c_str()));
    if (i >= 0 && kv.value().is<float>()) {
      newHas[i] = true;
      newVal[i] = kv.value().as<float>();
    }
  }
  bool nDht = false, nSoil = false, nLight = false;
  for (JsonVariantConst e : demo["force_errors"].as<JsonArrayConst>()) {
    String s = e.as<const char *>() ? e.as<const char *>() : "";
    if (s.startsWith("dht")) nDht = true;
    else if (s.startsWith("soil")) nSoil = true;
    else if (s.startsWith("light")) nLight = true;
  }
  for (int i = 0; i < F_COUNT; i++)
    if (newHas[i] != has[i] || (newHas[i] && newVal[i] != val[i])) changed = true;
  if (nDht != errDht || nSoil != errSoil || nLight != errLight) changed = true;

  if (newHas[F_SOIL] && (!has[F_SOIL] || newVal[F_SOIL] != val[F_SOIL])) soilKick = true;
  for (int i = 0; i < F_COUNT; i++) { has[i] = newHas[i]; val[i] = newVal[i]; }
  errDht = nDht; errSoil = nSoil; errLight = nLight;
  startTimer(ttl);
  applyTankIfNeeded(changed);
  if (changed) demoPrint();
}

void demoSetValue(const String &field, float value, uint32_t seconds) {
  int i = fieldIndex(field);
  if (i < 0) {
    Serial.println("[DEMO] unbekanntes Feld (soil|light|temp|hum|tank)");
    return;
  }
  has[i] = true;
  val[i] = value;
  if (i == F_SOIL) soilKick = true;
  startTimer(seconds);
  applyTankIfNeeded(true);
  demoPrint();
}

void demoForceError(const String &err, uint32_t seconds) {
  if (err.startsWith("dht")) errDht = true;
  else if (err.startsWith("soil")) errSoil = true;
  else if (err.startsWith("light")) errLight = true;
  else {
    Serial.println("[DEMO] unbekannter Fehler (dht|soil|light)");
    return;
  }
  startTimer(seconds);
  demoPrint();
}

void demoApply(Readings &r) {
  if (!demoActive()) return;
  if (has[F_SOIL]) { r.soil_pct = constrain(val[F_SOIL], 0.0f, 100.0f); r.soil_ok = true; r.demo_soil = true; }
  if (has[F_LIGHT]) { r.light_pct = constrain(val[F_LIGHT], 0.0f, 100.0f); r.light_ok = true; r.demo_light = true; }
  if (has[F_TEMP]) { r.temp_c = val[F_TEMP]; r.dht_ok = true; r.demo_temp = true; }
  if (has[F_HUM]) { r.hum_pct = constrain(val[F_HUM], 0.0f, 100.0f); r.dht_ok = true; r.demo_hum = true; }
  if (errDht) r.dht_ok = false;
  if (errSoil) r.soil_ok = false;
  if (errLight) r.light_ok = false;
}

void demoForcedErrors(JsonArray errors) {
  // Fehlercodes selbst werden aus soil_ok/light_ok/dht_ok erzeugt; hier nichts extra nötig.
  (void)errors;
}

void demoOverriddenFields(JsonArray fields) {
  if (!demoActive()) return;
  for (int i = 0; i < F_COUNT; i++)
    if (has[i]) fields.add(FIELD_NAMES[i]);
  if (errDht) { fields.add("air_temp_c"); fields.add("air_humidity_pct"); }
  if (errSoil) fields.add("soil_moisture_pct");
  if (errLight) fields.add("light_pct");
}

void demoPrint() {
  if (!active) {
    Serial.println("[DEMO] inaktiv");
    return;
  }
  Serial.print("[DEMO] aktiv, noch ");
  Serial.print((int32_t)(untilMs - millis()) / 1000);
  Serial.print(" s:");
  for (int i = 0; i < F_COUNT; i++)
    if (has[i]) Serial.printf(" %s=%.1f", FIELD_NAMES[i], val[i]);
  if (errDht) Serial.print(" FEHLER:dht");
  if (errSoil) Serial.print(" FEHLER:soil");
  if (errLight) Serial.print(" FEHLER:light");
  Serial.println();
}
