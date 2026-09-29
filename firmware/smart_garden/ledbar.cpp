// =============================================================================
//  ledbar.cpp – MY9221-Treiber (siehe ledbar.h)
// =============================================================================
#include "ledbar.h"

static uint8_t pinClk = 0, pinDat = 0;
static uint16_t ledOn = 0x00FF;   // Helligkeit einer eingeschalteten LED (8-Bit-Graustufe)
static bool slowEdges = false;     // 1-µs-Pausen zwischen den Flanken
static bool latchClock = true;     // Latch-Variante der Seeed-Bibliothek v2 (mit Taktimpulsen)

void ledbarSetOptions(uint8_t brightness, bool slow, bool latchWithClock) {
  ledOn = brightness ? brightness : 1;
  slowEdges = slow;
  latchClock = latchWithClock;
}

static inline void edgePause() {
  if (slowEdges) delayMicroseconds(1);
}

// Schiebt 16 Bit (MSB zuerst) hinaus. Der MY9221 übernimmt bei jeder Flanke
// (steigend UND fallend) ein Bit, deshalb wird der Takt nur umgeschaltet.
// Pro ledbarShow() werden 13 × 16 = 208 Flanken erzeugt (gerade Zahl), der
// Takt endet also immer wieder LOW.
static void send16(uint16_t bits) {
  static bool clk = false;
  for (int i = 0; i < 16; i++) {
    digitalWrite(pinDat, (bits & 0x8000) ? HIGH : LOW);
    edgePause();
    clk = !clk;
    digitalWrite(pinClk, clk ? HIGH : LOW);
    edgePause();
    bits <<= 1;
  }
}

// Latch: Takt stehen lassen, Daten LOW, >220 µs warten, 4 Impulse auf der
// Datenleitung -> der MY9221 übernimmt die gesendeten Werte.
// Variante "latchClock" = wie Seeed-Bibliothek v2 (zusätzliche Taktimpulse davor/danach).
static void latch() {
  digitalWrite(pinDat, LOW);
  if (latchClock) {
    digitalWrite(pinClk, HIGH);
    digitalWrite(pinClk, LOW);
    digitalWrite(pinClk, HIGH);
    digitalWrite(pinClk, LOW);
  }
  delayMicroseconds(240);
  for (int i = 0; i < 4; i++) {
    digitalWrite(pinDat, HIGH);
    edgePause();
    digitalWrite(pinDat, LOW);
    edgePause();
  }
  delayMicroseconds(1);
  if (latchClock) {
    digitalWrite(pinClk, HIGH);
    digitalWrite(pinClk, LOW);
  }
  delayMicroseconds(240);
}

void ledbarBegin(uint8_t pinClock, uint8_t pinData) {
  pinClk = pinClock;
  pinDat = pinData;
  pinMode(pinClk, OUTPUT);
  pinMode(pinDat, OUTPUT);
  digitalWrite(pinClk, LOW);
  digitalWrite(pinDat, LOW);
}

void ledbarShow(uint16_t bits, bool reverse) {
  send16(0x0000);                    // Kommando: 8-Bit-Graustufen, normaler Modus
  for (int ch = 0; ch < 12; ch++) {  // MY9221 hat 12 Kanäle, die Bar nutzt 10
    int seg = reverse ? (9 - ch) : ch;
    bool on = (ch < 10) && (bits & (1u << seg));
    send16(on ? ledOn : 0x0000);
  }
  latch();
}
