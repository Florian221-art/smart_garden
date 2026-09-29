// =============================================================================
//  demo.cpp – Demo-Modus (siehe demo.h)
// =============================================================================
#include "demo.h"
#include "garden_config.h"
#include "tank.h"

// Felder, die überschrieben werden dürfen (Namen wie im API-Vertrag)
enum Field { F_SOIL, F_LIGHT, F_TEMP, F_HUM, F_TANK, F_COUNT };
static const char *FIELD_NAMES[F_COUNT] = {"soil_moisture_pct", "light_pct", "air_temp_c", "air_humidity_pct",
                                           "water_level_pct"};
// Erlaubte Wertebereiche je Feld – schützt vor Unsinn vom Server oder Tippfehlern
static const float FIELD_MIN[F_COUNT] = {0, 0, -20, 0, 0};
static const float FIELD_MAX[F_COUNT] = {100, 100, 60, 100, 100};

static bool has[F_COUNT] = {false};  // Feld wird gerade überschrieben?
static float val[F_COUNT] = {0};     // Demo-Wert
static bool errDht = false, errSoil = false, errLight = false;  // erzwungene Ausfälle
static uint32_t untilMs = 0;         // Ablaufzeitpunkt (millis)
static bool active = false;
static bool soilKick = false;        // neue Bodenfeuchte-Vorgabe -> einmal sofort gießen

bool demoTakeSoilKick() {
  bool k = soilKick;
  soilKick = false;
  return k;
}

// Vertragsname oder Kurzform (Konsole) -> Index, -1 = unbekannt
static int fieldIndex(const String &name) {
  for (int i = 0; i < F_COUNT; i++)
    if (name == FIELD_NAMES[i]) return i;
  if (name == "soil") return F_SOIL;
  if (name == "light") return F_LIGHT;
  if (name == "temp") return F_TEMP;
  if (name == "hum") return F_HUM;
  if (name == "tank") return F_TANK;
  return -1;
}

static float clampField(int i, float v) { return constrain(v, FIELD_MIN[i], FIELD_MAX[i]); }

static void clearAll() {
  for (int i = 0; i < F_COUNT; i++) has[i] = false;
  errDht = errSoil = errLight = false;
  active = false;
  untilMs = 0;
  soilKick = false;
  tankSetDemoPct(-1);  // Demo-Füllstand verwerfen -> wieder echte Schätzung
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
  if (active && (int32_t)(millis() - untilMs) >= 0) {  // überlaufsicherer Vergleich
    Serial.println("[DEMO] abgelaufen – wieder echte Messwerte");
    clearAll();
  }
  return active;
}

void demoApplyFromServer(JsonVariantConst demo) {
  if (demo.isNull()) {  // Server sagt: keine Demo
    if (active) demoOff();
    return;
  }
  uint32_t ttl = demo["expires_in_s"] | 120;
  bool newHas[F_COUNT] = {false};
  float newVal[F_COUNT] = {0};
  for (JsonPairConst kv : demo["overrides"].as<JsonObjectConst>()) {
    int i = fieldIndex(String(kv.key().c_str()));
    if (i >= 0 && kv.value().is<float>()) {  // unbekannte Felder und Nicht-Zahlen ignorieren
      float v = kv.value().as<float>();
      if (isnan(v) || isinf(v)) continue;
      newHas[i] = true;
      newVal[i] = clampField(i, v);
    }
  }
  bool nDht = false, nSoil = false, nLight = false;
  for (JsonVariantConst e : demo["force_errors"].as<JsonArrayConst>()) {
    const char *s = e.as<const char *>();
    if (!s) continue;
    if (strncmp(s, "dht", 3) == 0) nDht = true;
    else if (strncmp(s, "soil", 4) == 0) nSoil = true;
    else if (strncmp(s, "light", 5) == 0) nLight = true;
  }

  // Nur bei echten Änderungen loggen (der Server schickt die Demo bei jeder Antwort)
  bool changed = nDht != errDht || nSoil != errSoil || nLight != errLight;
  for (int i = 0; i < F_COUNT; i++)
    if (newHas[i] != has[i] || (newHas[i] && newVal[i] != val[i])) changed = true;
  if (newHas[F_SOIL] && (!has[F_SOIL] || newVal[F_SOIL] != val[F_SOIL])) soilKick = true;

  for (int i = 0; i < F_COUNT; i++) {
    has[i] = newHas[i];
    val[i] = newVal[i];
  }
  errDht = nDht;
  errSoil = nSoil;
  errLight = nLight;
  if (!has[F_TANK]) tankSetDemoPct(-1);
  startTimer(ttl);
  if (changed) demoPrint();
}

void demoSetValue(const String &field, float value, uint32_t seconds) {
  int i = fieldIndex(field);
  if (i < 0) {
    Serial.println("[DEMO] unbekanntes Feld (soil|light|temp|hum|tank)");
    return;
  }
  if (!isfinite(value)) {
    Serial.println("[DEMO] ungültiger Wert");
    return;
  }
  has[i] = true;
  val[i] = clampField(i, value);
  if (i == F_SOIL) soilKick = true;
  startTimer(seconds);
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
  if (has[F_SOIL]) { r.soil_pct = val[F_SOIL]; r.soil_ok = true; r.demo_soil = true; }
  if (has[F_LIGHT]) { r.light_pct = val[F_LIGHT]; r.light_ok = true; r.demo_light = true; }
  if (has[F_TEMP]) { r.temp_c = val[F_TEMP]; r.dht_ok = true; r.demo_temp = true; }
  if (has[F_HUM]) { r.hum_pct = val[F_HUM]; r.dht_ok = true; r.demo_hum = true; }
  tankSetDemoPct(has[F_TANK] ? val[F_TANK] : -1.0f);
  if (errDht) r.dht_ok = false;
  if (errSoil) r.soil_ok = false;
  if (errLight) r.light_ok = false;
}

void demoOverriddenFields(JsonArray fields) {
  if (!demoActive()) return;
  for (int i = 0; i < F_COUNT; i++)
    if (has[i]) fields.add(FIELD_NAMES[i]);
  if (errDht) {
    if (!has[F_TEMP]) fields.add("air_temp_c");
    if (!has[F_HUM]) fields.add("air_humidity_pct");
  }
  if (errSoil && !has[F_SOIL]) fields.add("soil_moisture_pct");
  if (errLight && !has[F_LIGHT]) fields.add("light_pct");
}

void demoPrint() {
  if (!active) {
    Serial.println("[DEMO] inaktiv");
    return;
  }
  Serial.printf("[DEMO] aktiv, noch %ld s:", (long)((int32_t)(untilMs - millis()) / 1000));
  for (int i = 0; i < F_COUNT; i++)
    if (has[i]) Serial.printf(" %s=%.1f", FIELD_NAMES[i], val[i]);
  if (errDht) Serial.print(" FEHLER:dht");
  if (errSoil) Serial.print(" FEHLER:soil");
  if (errLight) Serial.print(" FEHLER:light");
  Serial.println();
}
