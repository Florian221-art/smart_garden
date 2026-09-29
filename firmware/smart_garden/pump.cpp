// =============================================================================
//  pump.cpp – Pumpensteuerung (siehe pump.h)
// =============================================================================
#include "pump.h"
#include "garden_config.h"
#include "tank.h"

static bool running = false;
static uint32_t startMs = 0;      // millis() beim Einschalten
static uint32_t durationMs = 0;   // geplante Laufzeit
static uint32_t lastStopMs = 0;   // millis() beim letzten Ausschalten
static bool everRan = false;      // gilt ab pumpBegin() als "gerade gelaufen" (siehe dort)
static float onSinceLast = 0;     // Laufzeit seit dem letzten erfolgreichen POST
static float todaySeconds = 0;    // Laufzeit im aktuellen 24-h-Fenster
static uint32_t dayStartMs = 0;

// Alle Zeitvergleiche arbeiten mit (millis() - start) als uint32_t – das
// bleibt auch beim Überlauf von millis() nach ~49 Tagen korrekt.

void pumpBegin() {
  digitalWrite(PIN_RELAY, LOW);  // Ausgangspegel festlegen, BEVOR der Pin Ausgang wird
  pinMode(PIN_RELAY, OUTPUT);
  digitalWrite(PIN_RELAY, LOW);
  dayStartMs = millis();
  // Nach jedem (Neu-)Start so tun, als wäre die Pumpe gerade gelaufen: Die
  // Auto-Bewässerung wartet erst pump_cooldown_s. Sonst könnte eine
  // Neustart-Schleife (z. B. Watchdog) bei jedem Start sofort pumpen – das
  // Tageslimit liegt nur im RAM und beginnt nach einem Neustart neu.
  everRan = true;
  lastStopMs = millis();
}

static uint32_t sinceLastStopMs() { return millis() - lastStopMs; }

bool pumpCooldownActive() {
  return everRan && sinceLastStopMs() < settings.pump_cooldown_s * 1000UL;
}

uint32_t pumpCooldownLeftS() {
  if (!pumpCooldownActive()) return 0;
  return settings.pump_cooldown_s - sinceLastStopMs() / 1000UL;
}

bool pumpStart(float seconds, const char *reason, bool respectCooldown, bool limitToSetting) {
  if (running) return false;
  if (!(seconds > 0) || isinf(seconds)) return false;  // 0, negativ, NaN, unendlich
  if (tankIsEmpty()) {
    Serial.printf("[PUMPE] blockiert (%s): Tank leer – Trockenlaufschutz. Nach dem Auffüllen \"refill\".\n", reason);
    return false;
  }
  // Harte Mindestpause gilt für JEDEN Start – auch Dashboard- und Demo-Befehle.
  // Das verhindert, dass viele Befehle hintereinander die Pumpe dauerhaft laufen lassen.
  if (everRan && sinceLastStopMs() < HARD_MIN_COOLDOWN_S * 1000UL) {
    Serial.printf("[PUMPE] blockiert (%s): Mindestpause %lu s\n", reason, (unsigned long)HARD_MIN_COOLDOWN_S);
    return false;
  }
  if (respectCooldown && pumpCooldownActive()) return false;

  if (limitToSetting) seconds = min(seconds, settings.max_pump_s_per_run);
  seconds = min(seconds, HARD_MAX_PUMP_S_PER_RUN);
  float dayLimit = min(settings.max_pump_s_per_day, HARD_MAX_PUMP_S_PER_DAY);
  float left = dayLimit - todaySeconds;
  if (left <= 0.1f) {
    Serial.printf("[PUMPE] blockiert (%s): Tageslimit %.0f s erreicht\n", reason, dayLimit);
    return false;
  }
  seconds = min(seconds, left);

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
  // Sicherheitsnetz: Relais muss AUS sein, wenn keine Pumpe laufen soll
  if (!running && digitalRead(PIN_RELAY) == HIGH) digitalWrite(PIN_RELAY, LOW);
  // Tageszähler alle 24 h Laufzeit zurücksetzen. Der ESP hat keine Uhr; nach
  // einem Neustart beginnt das Fenster neu (bekannte Einschränkung, siehe Doku).
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
