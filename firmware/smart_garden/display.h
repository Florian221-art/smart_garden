// display.h – LED-Bar (Bodenfeuchte Rot->Grün), Summer, Status-LED
#pragma once
#include <Arduino.h>

void displayBegin();
void displayUpdate();                  // in loop() aufrufen
void displaySetSoil(float pct, bool ok);
void displaySetTankEmpty(bool empty);
void displayIdentify();                // 5 s Lauflicht
void displayLedTest();                 // füllt die Bar einmal von Segment 1 bis 10
void displayFlip();                    // Richtung der LED-Bar umdrehen (wird gespeichert)

void buzzerBeep(uint8_t count);        // kurze Pieptöne (nicht blockierend)
void buzzerAlarm();                    // Alarm, höchstens einmal pro ALARM_REPEAT_MS

void statusLedSet(uint8_t mode);       // 0 = aus, 1 = an, 2 = langsam blinken, 3 = schnell blinken
