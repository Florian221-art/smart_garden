// =============================================================================
//  display.cpp – LED-Bar, Summer, Status-LED (siehe display.h)
// =============================================================================
#include "display.h"
#include "garden_config.h"
#include "ledbar.h"

// Bits: Bit 0 = Segment 1 (rot) ... Bit 9 = Segment 10 (grün)
static const uint16_t ALL_SEGMENTS = 0x3FF;

// --- LED-Bar -----------------------------------------------------------------
static float soilPct = 0;
static bool soilOk = true;
static bool tankEmpty = false;
static uint32_t identifyStart = 0;     // Lauflicht seit (millis)
static bool identifyOn = false;
static uint32_t lastFrame = 0;         // letzte Aktualisierung (alle 100 ms)
static uint32_t lastBits = 0xFFFFFFFF; // zuletzt gesendetes Muster (nur bei Änderung senden)
static uint32_t testStart = 0;         // "ledseg"-Test seit (millis)
static bool testOn = false;
static uint16_t testBits = 0;
static uint32_t ledTestStart = 0;      // Start der Testanimation, 0 = keine
static bool ledTestRunning = false;
static uint32_t diagStart = 0;         // "leddiag" seit (millis)
static bool diagOn = false;
static int diagShown = -1;             // zuletzt angezeigte Variante

// --- Summer --------------------------------------------------------------------
static uint8_t beepsLeft = 0;
static bool buzzerOn = false;
static uint32_t buzzerNext = 0;
static uint32_t lastAlarm = 0;
static uint8_t alarmed = 0;             // Probleme, für die schon gepiept wurde
static uint32_t clearedSince[8] = {0};  // seit wann ein Problem weg ist (0 = besteht)

// --- Status-LED -----------------------------------------------------------------
static uint8_t ledMode = 0;

// Zeitfenster mit Flag + Startzeit statt "bis"-Zeitstempel: bleibt auch nach
// dem Überlauf von millis() (49 Tage) korrekt. Liefert false und beendet das
// Fenster, sobald `durationMs` vorbei ist.
static bool within(bool &on, uint32_t start, uint32_t durationMs) {
  if (on && millis() - start >= durationMs) on = false;
  return on;
}

static uint32_t lastSendMs = 0;

// Neu senden bei Änderung und zusätzlich jede Sekunde: Falls ein Rahmen durch
// eine Störung falsch ankam, stimmt die Anzeige spätestens 1 s später wieder.
static void showBits(uint32_t bits) {
  if (bits != lastBits || millis() - lastSendMs >= 1000) {
    ledbarShow(bits, CFG_LEDBAR_REVERSE);
    lastBits = bits;
    lastSendMs = millis();
  }
}

static void applyPinsSwap(bool swap) {
  if (swap) ledbarBegin(PIN_LEDBAR_DI, PIN_LEDBAR_DCKI);
  else ledbarBegin(PIN_LEDBAR_DCKI, PIN_LEDBAR_DI);
  lastBits = 0xFFFFFFFF;
}

static void applyPins() {
  applyPinsSwap(CFG_LEDBAR_SWAP_PINS);
  ledbarSetOptions(CFG_LEDBAR_BRIGHTNESS, CFG_LEDBAR_SLOW, CFG_LEDBAR_LATCH_CLOCK);
}

// Fehlersuche: 8 Varianten (Pins normal/getauscht × hell+schnell/gedimmt+langsam
// × Latch mit/ohne Takt). Jede zeigt 3 s lang Segment 1-3 (rot, orange, 1x grün),
// dann 1 s alles aus. Die Variante, die genau das zeigt, ist die richtige.
void displayDiag() {
  diagStart = millis();
  diagOn = true;
  diagShown = -1;
  Serial.println("[LED] Diagnose: 8 Varianten je 4 s. Richtig ist die, bei der GENAU rot + orange + 1 gruen leuchten.");
}

static void diagVariant(int v, bool &swap, uint8_t &bright, bool &slow, bool &latchClk) {
  swap = v & 1;
  bright = (v & 2) ? 0x40 : 0xFF;
  slow = (v & 2);
  latchClk = !(v & 4);
}

void displayBegin() {
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  pinMode(PIN_STATUS_LED, OUTPUT);
  applyPins();
  showBits(0);
}

void displaySetSoil(float pct, bool ok) {
  soilPct = pct;
  soilOk = ok;
}

void displaySetTankEmpty(bool empty) { tankEmpty = empty; }

void displayIdentify() {
  identifyStart = millis();
  identifyOn = true;
}

void displayFlip() {
  CFG_LEDBAR_REVERSE = !CFG_LEDBAR_REVERSE;
  lastBits = 0xFFFFFFFF;
  Serial.printf("[LED] Richtung umgedreht (reverse=%d). Dauerhaft: oben im Sketch CFG_LEDBAR_REVERSE = %s setzen.\n",
                CFG_LEDBAR_REVERSE, CFG_LEDBAR_REVERSE ? "true" : "false");
}

void displaySwapPins() {
  CFG_LEDBAR_SWAP_PINS = !CFG_LEDBAR_SWAP_PINS;
  applyPins();
  Serial.printf("[LED] Pins getauscht (swap=%d): Takt=GPIO%d, Daten=GPIO%d. Reagiert die Bar jetzt, oben CFG_LEDBAR_SWAP_PINS = %s setzen.\n",
                CFG_LEDBAR_SWAP_PINS, CFG_LEDBAR_SWAP_PINS ? PIN_LEDBAR_DI : PIN_LEDBAR_DCKI,
                CFG_LEDBAR_SWAP_PINS ? PIN_LEDBAR_DCKI : PIN_LEDBAR_DI, CFG_LEDBAR_SWAP_PINS ? "true" : "false");
  displayLedTest();
}

void displayTestSegments(int n) {
  n = constrain(n, 0, 10);
  testBits = (1u << n) - 1;
  testStart = millis();
  testOn = true;
  Serial.printf("[LED] Test: %d Segment(e) an (ab Segment 1 = rot) fuer 10 s\n", n);
}

void displayLedTest() {
  Serial.println("[LED] Test: fuellt von GRUEN (Segment 10) ueber orange bis ROT (Segment 1).");
  Serial.println("[LED] Leuchtet zuerst ein ROTES Segment -> oben CFG_LEDBAR_REVERSE = true setzen.");
  ledTestStart = millis();
  ledTestRunning = true;
}

// Berechnet das Muster für die Bodenfeuchte-Anzeige
static uint32_t soilBits(bool blinkSlow) {
  int pos = constrain((int)ceilf(soilPct / 10.0f), 1, 10);  // 0-10 % -> 1, ..., 90-100 % -> 10
  uint32_t bits;
  if (CFG_LEDBAR_MODE == 2) {
    // Trockenheitsbalken, vom grünen Ende her gefüllt (Florians Tabelle):
    //   100 % -> 1 grüne, 90 % -> 2, ... 40 % -> 7, 30 % -> 8 grüne,
    //   20 % -> 8 grüne + orange, unter 10 % -> alle inkl. rot
    int n = constrain((int)floorf((100.0f - soilPct) / 10.0f) + 1, 1, 10);
    bits = (ALL_SEGMENTS << (10 - n)) & ALL_SEGMENTS;  // Segmente (11-n) bis 10
  } else {
    if (CFG_LEDBAR_MODE == 1) {
      bits = (1UL << pos) - 1;                   // Füllbalken: Segmente 1..pos
    } else {
      bits = 1UL << (pos - 1);                   // Zeiger: Segment pos ...
      if (pos > 1) bits |= 1UL << (pos - 2);     // ... und das davor
    }
    if (soilPct < 10 && !blinkSlow) bits = 0;    // sehr trocken: blinkt
  }
  return bits;
}

static void updateBar() {
  uint32_t now = millis();
  if (now - lastFrame < 100) return;  // 10 Bilder pro Sekunde reichen
  lastFrame = now;
  bool blinkFast = (now / 250) % 2;
  bool blinkSlow = (now / 500) % 2;

  // Priorität: Diagnose > Testanimation > ledseg-Test > Lauflicht > Sensorfehler > Bodenfeuchte
  if (diagOn) {
    uint32_t t = now - diagStart;
    int v = t / 4000;
    if (v < 8) {
      bool swap, slow, latchClk;
      uint8_t bright;
      diagVariant(v, swap, bright, slow, latchClk);
      if (v != diagShown) {
        diagShown = v;
        applyPinsSwap(swap);
        ledbarSetOptions(bright, slow, latchClk);
        Serial.printf("[LED] Variante %d: Pins %s, Helligkeit %s, Latch %s\n", v + 1, swap ? "getauscht" : "normal",
                      bright == 0xFF ? "voll/schnell" : "gedimmt/langsam", latchClk ? "mit Takt" : "ohne Takt");
      }
      ledbarShow((t % 4000) < 3000 ? 0x007 : 0x000, CFG_LEDBAR_REVERSE);  // jedes Bild neu senden
      return;
    }
    diagOn = false;
    applyPins();
    Serial.println("[LED] Diagnose fertig. Bitte melden, welche Variante(n) richtig waren.");
  }
  if (ledTestRunning) {
    uint32_t step = (now - ledTestStart) / 250;  // alle 250 ms ein Segment mehr
    if (step < 10) {
      int n = step + 1;
      showBits((ALL_SEGMENTS << (10 - n)) & ALL_SEGMENTS);
      return;
    }
    ledTestRunning = false;  // danach direkt die echte Anzeige (nicht alle LEDs stehen lassen)
  }
  if (within(testOn, testStart, 10000)) {
    showBits(testBits);
    return;
  }
  if (within(identifyOn, identifyStart, 5000)) {
    showBits(1UL << ((now / 100) % 10));
    return;
  }
  if (!soilOk) {
    showBits(blinkSlow ? 0x001 : 0x200);
    return;
  }
  uint32_t bits = soilBits(blinkSlow);
  if (tankEmpty) bits = blinkFast ? (bits | 0x001) : (bits & ~0x001UL);  // Tank leer: Segment 1 blinkt schnell
  showBits(bits);
}

// --- Summer ------------------------------------------------------------------

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
  // Ein Problem gilt erst als erledigt, wenn es 5 min lang weg war.
  // Sonst würde ein Wackelkontakt bei jedem Wiederauftreten neu piepen.
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
  Serial.printf("[ALARM]%s%s%s%s\n", (problems & ALARM_TANK_EMPTY) ? " Tank leer (\"refill\" eingeben)" : "",
                (problems & ALARM_SOIL_SENSOR) ? " Bodensensor-Fehler (Kabel D34 oder Kalibrierung pruefen)" : "",
                (problems & ALARM_DHT) ? " DHT11 liefert seit >60 s keine Werte (Kabel D4/3V3/GND pruefen)" : "",
                (problems & ALARM_WATERING_INEFFECTIVE)
                    ? " Giessen wirkt nicht (Sensor in der Erde? Schlauch im Kuebel? Pumpe im Wasser?) -> Auto-Giessen gesperrt, \"refill\" hebt auf"
                    : "");
  buzzerBeep(3);
}

// Erzeugt die Pieptöne: 150 ms an, 150 ms aus
static void updateBuzzer() {
  uint32_t now = millis();
  if (!buzzerOn && beepsLeft == 0) return;
  if ((int32_t)(now - buzzerNext) < 0) return;
  if (buzzerOn) {
    digitalWrite(PIN_BUZZER, LOW);
    buzzerOn = false;
    buzzerNext = now + 150;
  } else if (beepsLeft > 0) {
    digitalWrite(PIN_BUZZER, HIGH);
    buzzerOn = true;
    beepsLeft--;
    buzzerNext = now + 150;
  }
}

// --- Status-LED ----------------------------------------------------------------

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
