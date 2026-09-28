#include "pump.h"
#include "garden_config.h"
#include "tank.h"

static bool running = false;
static uint32_t startMs = 0;
static uint32_t durationMs = 0;
static uint32_t lastStopMs = 0;
static bool everRan = false;
static float onSinceLast = 0;
static float todaySeconds = 0;
static uint32_t dayStartMs = 0;

void pumpBegin() {
  digitalWrite(PIN_RELAY, LOW);  // sicher AUS
  pinMode(PIN_RELAY, OUTPUT);
  digitalWrite(PIN_RELAY, LOW);
  dayStartMs = millis();
}

bool pumpCooldownActive() {
  return everRan && (millis() - lastStopMs) < settings.pump_cooldown_s * 1000UL;
}

uint32_t pumpCooldownLeftS() {
  if (!pumpCooldownActive()) return 0;
  return settings.pump_cooldown_s - (millis() - lastStopMs) / 1000UL;
}

bool pumpStart(float seconds, const char *reason, bool respectCooldown) {
  if (running) return false;
  if (tankIsEmpty()) {
    Serial.printf("[PUMPE] blockiert (%s): Tank leer – Trockenlaufschutz\n", reason);
    return false;
  }
  if (respectCooldown && pumpCooldownActive()) return false;
  seconds = min(seconds, settings.max_pump_s_per_run);
  seconds = min(seconds, HARD_MAX_PUMP_S_PER_RUN);
  float left = settings.max_pump_s_per_day - todaySeconds;
  if (left <= 0.1f) {
    Serial.printf("[PUMPE] blockiert (%s): Tageslimit %.0f s erreicht\n", reason, settings.max_pump_s_per_day);
    return false;
  }
  seconds = min(seconds, left);
  if (seconds <= 0) return false;

  durationMs = (uint32_t)(seconds * 1000.0f);
  startMs = millis();
  running = true;
  digitalWrite(PIN_RELAY, HIGH);
  Serial.printf("[PUMPE] AN für %.1f s (%s)\n", seconds, reason);
  return true;
}

void pumpStop() {
  if (!running) return;
  digitalWrite(PIN_RELAY, LOW);
  running = false;
  float s = (millis() - startMs) / 1000.0f;
  onSinceLast += s;
  todaySeconds += s;
  lastStopMs = millis();
  everRan = true;
  tankConsume(s);
  Serial.printf("[PUMPE] AUS nach %.1f s, Tank %.0f ml (%.0f %%)\n", s, tankRemainingMl(), tankLevelPct());
}

void pumpUpdate() {
  if (running && (millis() - startMs >= durationMs)) pumpStop();
  // Tageszähler alle 24 h Laufzeit zurücksetzen (keine Uhr auf dem ESP)
  if (millis() - dayStartMs >= 86400000UL) {
    dayStartMs = millis();
    todaySeconds = 0;
  }
}

bool pumpRunning() { return running; }

float pumpTakeOnSecondsSinceLast() {
  float s = onSinceLast;
  onSinceLast = 0;
  return s;
}

void pumpRestoreOnSeconds(float s) { onSinceLast += s; }

float pumpTodaySeconds() { return todaySeconds; }
