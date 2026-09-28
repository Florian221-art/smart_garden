// tank.h – geschätzter Füllstand (kein Sensor): Restmenge -= Pumpenlaufzeit × Durchfluss
#pragma once
#include <Arduino.h>

void tankBegin();
void tankConsume(float pump_seconds);  // nach jedem Pumpenlauf
void tankRefill();                     // Tank voll (Server-Befehl, BOOT-Taste, seriell)
void tankSetPct(float pct);            // Demo-Modus
float tankRemainingMl();
float tankLevelPct();
bool tankIsEmpty();                    // <= TANK_EMPTY_PCT
bool tankIsLow();                      // <= settings.tank_low_pct
