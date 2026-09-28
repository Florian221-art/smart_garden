// ledbar.h – eigener Treiber für die Grove LED Bar v2.0 (Chip MY9221)
// Kein externes Library nötig (die Grove-Library gibt es in mehreren inkompatiblen Versionen).
#pragma once
#include <Arduino.h>

void ledbarBegin(uint8_t pinClock, uint8_t pinData);
// bits: Bit 0 = Segment 1 ... Bit 9 = Segment 10; reverse dreht die Reihenfolge um
void ledbarShow(uint16_t bits, bool reverse);
