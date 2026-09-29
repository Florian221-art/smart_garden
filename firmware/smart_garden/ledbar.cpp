// =============================================================================
//  ledbar.cpp – MY9221-Treiber (siehe ledbar.h)
// =============================================================================
#include "ledbar.h"

static uint8_t pinClk = 0, pinDat = 0;
// Helligkeit einer eingeschalteten LED (8-Bit-Graustufe, 0x00-0xFF).
// Bewusst NICHT voll (0xFF): Alle 10 LEDs mit voller Helligkeit ziehen so viel
// Strom aus der 3,3-V-Schiene, dass die Spannung einbricht und der MY9221 danach
// keine Daten mehr annimmt (Anzeige bleibt auf "alle an" hängen). ~25 % reicht
// zum Ablesen und schont die Versorgung.
static const uint16_t LED_ON = 0x0040;

// Schiebt 16 Bit (MSB zuerst) hinaus. Der MY9221 übernimmt bei jeder Flanke
// (steigend UND fallend) ein Bit, deshalb wird der Takt nur umgeschaltet.
// Pro ledbarShow() werden 13 × 16 = 208 Flanken erzeugt (gerade Zahl), der
// Takt endet also immer wieder LOW – passend zu latch().
// Kleine Pausen (1 µs) sorgen für saubere Flanken auf langen Grove-/Steckbrett-
// Kabeln; der ESP32 schaltet die Pins sonst schneller als nötig.
static void send16(uint16_t bits) {
  static bool clk = false;
  for (int i = 0; i < 16; i++) {
    digitalWrite(pinDat, (bits & 0x8000) ? HIGH : LOW);
    delayMicroseconds(1);
    clk = !clk;
    digitalWrite(pinClk, clk ? HIGH : LOW);
    delayMicroseconds(1);
    bits <<= 1;
  }
}

// Latch-Sequenz genau wie in der aktuellen Seeed-Bibliothek "Grove_LED_Bar"
// (mit der sie auf diesem Board nachweislich funktioniert hat): Daten LOW,
// zwei Taktimpulse, >220 µs warten, 4 Impulse auf der Datenleitung, dann ein
// letzter Taktimpuls -> der MY9221 übernimmt die gesendeten Werte.
static void latch() {
  digitalWrite(pinDat, LOW);
  digitalWrite(pinClk, HIGH);
  digitalWrite(pinClk, LOW);
  digitalWrite(pinClk, HIGH);
  digitalWrite(pinClk, LOW);
  delayMicroseconds(240);
  for (int i = 0; i < 4; i++) {
    digitalWrite(pinDat, HIGH);
    delayMicroseconds(1);
    digitalWrite(pinDat, LOW);
    delayMicroseconds(1);
  }
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
  send16(0x0000);                    // Kommando: 8-Bit-Graustufen, normaler Modus
  for (int ch = 0; ch < 12; ch++) {  // MY9221 hat 12 Kanäle, die Bar nutzt 10
    int seg = reverse ? (9 - ch) : ch;
    bool on = (ch < 10) && (bits & (1u << seg));
    send16(on ? LED_ON : 0x0000);
  }
  latch();
}
