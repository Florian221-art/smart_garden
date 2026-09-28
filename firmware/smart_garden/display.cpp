#include "display.h"
#include "garden_config.h"
#include "ledbar.h"

static float soilPct = 0;
static bool soilOk = true;
static bool tankEmpty = false;
static uint32_t identifyUntil = 0;
static uint32_t lastFrame = 0;
static uint32_t lastBits = 0xFFFFFFFF;
static uint32_t testUntil = 0;
static uint16_t testBits = 0;

// Summer
static uint8_t beepsLeft = 0;
static bool buzzerOn = false;
static uint32_t buzzerNext = 0;
static uint32_t lastAlarm = 0;
static uint8_t lastProblems = 0;

// Status-LED
static uint8_t ledMode = 0;

static void showBits(uint32_t bits) {
  if (bits != lastBits) {
    ledbarShow(bits, CFG_LEDBAR_REVERSE);
    lastBits = bits;
  }
}

void displayBegin() {
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  pinMode(PIN_STATUS_LED, OUTPUT);
  ledbarBegin(PIN_LEDBAR_DCKI, PIN_LEDBAR_DI);
  showBits(0);
}

void displaySetSoil(float pct, bool ok) {
  soilPct = pct;
  soilOk = ok;
}

void displaySetTankEmpty(bool empty) { tankEmpty = empty; }

void displayIdentify() { identifyUntil = millis() + 5000; }

void displayFlip() {
  CFG_LEDBAR_REVERSE = !CFG_LEDBAR_REVERSE;
  lastBits = 0xFFFFFFFF;
  Serial.printf("[LED] Richtung umgedreht (reverse=%d). Dauerhaft: oben im Sketch CFG_LEDBAR_REVERSE = %s setzen.\n",
                CFG_LEDBAR_REVERSE, CFG_LEDBAR_REVERSE ? "true" : "false");
}

void displayTestSegments(int n) {
  n = constrain(n, 0, 10);
  testBits = (1u << n) - 1;
  testUntil = millis() + 10000;
  Serial.printf("[LED] Test: %d Segment(e) an (ab Segment 1) fuer 10 s\n", n);
}

void displayLedTest() {
  Serial.println("[LED] Test: Segment 1 (ROT) bis 10 (gruen) – falls gruen zuerst leuchtet: CFG_LEDBAR_REVERSE = true");
  for (int i = 1; i <= 10; i++) {
    ledbarShow((1u << i) - 1, CFG_LEDBAR_REVERSE);
    delay(150);
  }
  delay(500);
  lastBits = 0xFFFFFFFF;
}

static void updateBar() {
  uint32_t now = millis();
  if (now - lastFrame < 100) return;
  lastFrame = now;
  bool blinkFast = (now / 250) % 2;
  bool blinkSlow = (now / 500) % 2;

  if ((int32_t)(testUntil - now) > 0) {  // Test "ledseg"
    showBits(testBits);
    return;
  }
  if ((int32_t)(identifyUntil - now) > 0) {  // Lauflicht
    showBits(1UL << ((now / 100) % 10));
    return;
  }
  if (!soilOk) {  // Sensorfehler: Segment 1 und 10 abwechselnd
    showBits(blinkSlow ? 0x001 : 0x200);
    return;
  }
  // Bodenfeuchte -> Segmente (1 = rot = trocken, 10 = grün = nass)
  int pos = (int)ceilf(soilPct / 10.0f);
  pos = constrain(pos, 1, 10);
  uint32_t bits;
  if (CFG_LEDBAR_MODE == 1) {
    bits = (1UL << pos) - 1;                      // Füllbalken: Segmente 1..pos
  } else {
    bits = (1UL << (pos - 1));                    // Zeiger: Segment pos ...
    if (pos > 1) bits |= (1UL << (pos - 2));      // ... und das davor
  }
  if (soilPct < 10 && !blinkSlow) bits = 0;       // sehr trocken: rot blinkt
  if (tankEmpty) bits = blinkFast ? (bits | 0x001) : (bits & ~0x001UL);  // Tank leer: Segment 1 blinkt schnell
  showBits(bits);
}

void buzzerBeepForced(uint8_t count) {
  beepsLeft = count;
  buzzerNext = millis();
}

void buzzerBeep(uint8_t count) {
  if (!settings.buzzer_enabled || !CFG_BUZZER_ENABLED) return;
  buzzerBeepForced(count);
}

void buzzerAlarm(uint8_t problems) {
  bool isNew = (problems & ~lastProblems) != 0;
  bool repeat = problems != 0 && millis() - lastAlarm >= ALARM_REPEAT_MS;
  lastProblems = problems;
  if (!problems || !(isNew || repeat)) return;
  lastAlarm = millis();
  Serial.printf("[ALARM]%s%s%s\n", (problems & 1) ? " Tank leer (\"refill\" eingeben)" : "",
                (problems & 2) ? " Bodensensor-Fehler" : "", (problems & 4) ? " DHT11-Fehler" : "");
  buzzerBeep(3);
}

static void updateBuzzer() {
  uint32_t now = millis();
  if (!buzzerOn && beepsLeft == 0) return;
  if ((int32_t)(now - buzzerNext) < 0) return;
  if (buzzerOn) {
    digitalWrite(PIN_BUZZER, LOW);
    buzzerOn = false;
    buzzerNext = now + 150;  // Pause
  } else if (beepsLeft > 0) {
    digitalWrite(PIN_BUZZER, HIGH);
    buzzerOn = true;
    beepsLeft--;
    buzzerNext = now + 150;  // Ton
  }
}

void statusLedSet(uint8_t mode) { ledMode = mode; }

static void updateStatusLed() {
  uint32_t now = millis();
  bool on = false;
  switch (ledMode) {
    case 1: on = true; break;
    case 2: on = (now / 500) % 2; break;
    case 3: on = (now / 100) % 2; break;
    default: on = false;
  }
  digitalWrite(PIN_STATUS_LED, on ? HIGH : LOW);
}

void displayUpdate() {
  updateBar();
  updateBuzzer();
  updateStatusLed();
}
