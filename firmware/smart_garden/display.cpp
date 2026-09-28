#include "display.h"
#include "garden_config.h"
#include "ledbar.h"

static float soilPct = 0;
static bool soilOk = true;
static bool tankEmpty = false;
static uint32_t identifyUntil = 0;
static uint32_t lastFrame = 0;
static uint32_t lastBits = 0xFFFFFFFF;

// Summer
static uint8_t beepsLeft = 0;
static bool buzzerOn = false;
static uint32_t buzzerNext = 0;
static uint32_t lastAlarm = 0;
static bool alarmEver = false;

// Status-LED
static uint8_t ledMode = 0;

static void showBits(uint32_t bits) {
  if (bits != lastBits) {
    ledbarShow(bits, calib.ledbar_green_to_red);
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
  calib.ledbar_green_to_red = !calib.ledbar_green_to_red;
  lastBits = 0xFFFFFFFF;
  configSaveCalibration();
  Serial.printf("[LED] Richtung umgedreht (greenToRed=%d)\n", calib.ledbar_green_to_red);
}

void displayLedTest() {
  Serial.println("[LED] Test: Segment 1 (soll ROT sein) bis 10 (grün)");
  for (int i = 1; i <= 10; i++) {
    ledbarShow((1u << i) - 1, calib.ledbar_green_to_red);
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

  if ((int32_t)(identifyUntil - now) > 0) {  // Lauflicht
    showBits(1UL << ((now / 100) % 10));
    return;
  }
  if (!soilOk) {  // Sensorfehler: Segment 1 und 10 abwechselnd
    showBits(blinkSlow ? 0x001 : 0x200);
    return;
  }
  // Bodenfeuchte -> Segmente (1 = rot = trocken, 10 = grün = nass)
  int segs = (int)ceilf(soilPct / 10.0f);
  segs = constrain(segs, 1, 10);
  uint32_t bits = (1UL << segs) - 1;
  if (soilPct < 10 && !blinkSlow) bits = 0;       // sehr trocken: rot blinkt
  if (tankEmpty) bits = blinkFast ? (bits | 0x001) : (bits & ~0x001UL);  // Tank leer: Segment 1 blinkt schnell
  showBits(bits);
}

void buzzerBeep(uint8_t count) {
  if (!settings.buzzer_enabled) return;
  beepsLeft = count;
  buzzerNext = millis();
}

void buzzerAlarm() {
  if (alarmEver && millis() - lastAlarm < ALARM_REPEAT_MS) return;
  lastAlarm = millis();
  alarmEver = true;
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
