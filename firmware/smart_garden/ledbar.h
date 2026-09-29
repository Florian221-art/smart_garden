// =============================================================================
//  ledbar.h – eigener Treiber für die Grove LED Bar v2.0 (Chip MY9221)
// =============================================================================
//  Warum ein eigener Treiber? Die Grove-Bibliothek gibt es in mehreren
//  inkompatiblen Versionen (die aus dem Bibliotheksverwalter kennt LED_BAR_10
//  nicht). Das Protokoll ist einfach: 16-Bit-Kommando + 12 × 16-Bit-Helligkeit,
//  Daten werden bei JEDER Taktflanke übernommen, danach ein Latch-Impuls.
// =============================================================================
#pragma once
#include <Arduino.h>

void ledbarBegin(uint8_t pinClock, uint8_t pinData);  // Pins als Ausgang, beide LOW
// bits: Bit 0 = Segment 1 (rot) ... Bit 9 = Segment 10 (grün).
// reverse = true spiegelt die Reihenfolge (falls die Bar andersherum eingebaut ist).
void ledbarShow(uint16_t bits, bool reverse);
// Übertragungsvarianten (für die Fehlersuche mit "leddiag"):
//   brightness 1-255, slow = 1-µs-Pausen zwischen den Flanken,
//   latchWithClock = Latch mit zusätzlichen Taktimpulsen (Seeed-Bibliothek v2)
void ledbarSetOptions(uint8_t brightness, bool slow, bool latchWithClock);
