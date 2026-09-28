#include "ledbar.h"

static uint8_t pinClk = 0, pinDat = 0;
static const uint16_t LED_ON = 0x00FF;  // volle Helligkeit (8 Graustufen-Bits)

static void send16(uint16_t bits) {
  static bool clk = false;
  for (int i = 0; i < 16; i++) {
    digitalWrite(pinDat, (bits & 0x8000) ? HIGH : LOW);
    clk = !clk;  // MY9221 übernimmt Daten bei jeder Flanke
    digitalWrite(pinClk, clk ? HIGH : LOW);
    bits <<= 1;
  }
}

static void latch() {
  digitalWrite(pinDat, LOW);
  digitalWrite(pinClk, HIGH); digitalWrite(pinClk, LOW);
  digitalWrite(pinClk, HIGH); digitalWrite(pinClk, LOW);
  delayMicroseconds(240);
  for (int i = 0; i < 4; i++) { digitalWrite(pinDat, HIGH); digitalWrite(pinDat, LOW); }
  delayMicroseconds(1);
  digitalWrite(pinClk, HIGH);
  digitalWrite(pinClk, LOW);
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
  send16(0x0000);  // Kommando
  for (int ch = 0; ch < 12; ch++) {  // MY9221 hat 12 Kanäle, die Bar nutzt 10
    int seg = reverse ? (9 - ch) : ch;
    bool on = (ch < 10) && (bits & (1u << seg));
    send16(on ? LED_ON : 0x0000);
  }
  latch();
}
