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
static uint8_t alarmed = 0;           // Probleme, für die schon gepiept wurde
static uint32_t clearedSince[8] = {0};  // seit wann ein Problem weg ist

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
  if (CFG_LEDBAR_SWAP_PINS) ledbarBegin(PIN_LEDBAR_DI, PIN_LEDBAR_DCKI);
  else ledbarBegin(PIN_LEDBAR_DCKI, PIN_LEDBAR_DI);
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

void displaySwapPins() {
  CFG_LEDBAR_SWAP_PINS = !CFG_LEDBAR_SWAP_PINS;
  if (CFG_LEDBAR_SWAP_PINS) ledbarBegin(PIN_LEDBAR_DI, PIN_LEDBAR_DCKI);
  else ledbarBegin(PIN_LEDBAR_DCKI, PIN_LEDBAR_DI);
  Serial.printf("[LED] Pins getauscht (swap=%d): Takt=GPIO%d, Daten=GPIO%d. Reagiert die Bar jetzt, oben CFG_LEDBAR_SWAP_PINS = %s setzen.\n",
                CFG_LEDBAR_SWAP_PINS, CFG_LEDBAR_SWAP_PINS ? PIN_LEDBAR_DI : PIN_LEDBAR_DCKI,
                CFG_LEDBAR_SWAP_PINS ? PIN_LEDBAR_DCKI : PIN_LEDBAR_DI, CFG_LEDBAR_SWAP_PINS ? "true" : "false");
  displayLedTest();
}

void displayTestSegments(int n) {
  n = constrain(n, 0, 10);
  testBits = (1u << n) - 1;
  testUntil = millis() + 10000;
  Serial.printf("[LED] Test: %d Segment(e) an (ab Segment 1) fuer 10 s\n", n);
}

void displayLedTest() {
  Serial.println("[LED] Test: fuellt von GRUEN (Segment 10) ueber orange bis ROT (Segment 1).");
  Serial.println("[LED] Leuchtet zuerst ein ROTES Segment -> oben CFG_LEDBAR_REVERSE = true setzen.");
  for (int i = 1; i <= 10; i++) {
    ledbarShow((0x3FFu << (10 - i)) & 0x3FFu, CFG_LEDBAR_REVERSE);
    delay(250);
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
  if (CFG_LEDBAR_MODE == 2) {
    // Trockenheitsbalken: Anzahl LEDs = Trockenheit, gefüllt vom grünen Ende (Segment 10) her.
    // 100 % feucht -> 1 grüne LED, 20 % -> alle 8 grünen, 10-20 % -> + orange, < 10 % -> + rot
    int n = (int)ceilf((100.0f - soilPct) / 10.0f);
    n = constrain(n, 1, 10);
    bits = (0x3FFUL << (10 - n)) & 0x3FFUL;       // Segmente (11-n)..10
    if (soilPct < 10 && !blinkSlow) bits &= ~0x001UL;  // ganz trocken: rot blinkt
  } else if (CFG_LEDBAR_MODE == 1) {
    bits = (1UL << pos) - 1;                      // Füllbalken: Segmente 1..pos
  } else {
    bits = (1UL << (pos - 1));                    // Zeiger: Segment pos ...
    if (pos > 1) bits |= (1UL << (pos - 2));      // ... und das davor
  }
  if (CFG_LEDBAR_MODE != 2 && soilPct < 10 && !blinkSlow) bits = 0;  // sehr trocken: rot blinkt
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
  uint32_t now = millis();
  // Ein Problem gilt erst als erledigt, wenn es 5 min lang weg war -> kein Dauerpiepen bei Wackelkontakt
  for (int b = 0; b < 8; b++) {
    uint8_t m = 1 << b;
    if (problems & m) clearedSince[b] = 0;
    else if (alarmed & m) {
      if (!clearedSince[b]) clearedSince[b] = now ? now : 1;
      else if (now - clearedSince[b] > 300000) alarmed &= ~m;
    }
  }
  bool isNew = (problems & ~alarmed) != 0;
  bool repeat = problems != 0 && now - lastAlarm >= ALARM_REPEAT_MS;
  alarmed |= problems;
  if (!problems || !(isNew || repeat)) return;
  lastAlarm = now;
  Serial.printf("[ALARM]%s%s%s\n", (problems & 1) ? " Tank leer (\"refill\" eingeben)" : "",
                (problems & 2) ? " Bodensensor-Fehler" : "", (problems & 4) ? " DHT11 liefert seit >60 s keine Werte (Kabel D4/3V3/GND pruefen)" : "");
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
