// =============================================================================
//  display.h – LED-Bar (Bodenfeuchte), Summer, Onboard-Status-LED
// =============================================================================
//  Alles hier ist nicht blockierend: Die Funktionen setzen nur Zustände,
//  displayUpdate() (in jedem loop()-Durchlauf) erzeugt daraus Blinken, Töne und
//  Animationen. So verzögert die Anzeige nie das Abschalten der Pumpe.
//
//  LED-Bar: Segment 1 = rot, Segment 2 = orange, Segmente 3-10 = grün.
//  Anzeige je nach CFG_LEDBAR_MODE (oben im Sketch):
//    2 = Trockenheitsbalken ab Grün (Standard): je trockener, desto mehr LEDs –
//        erst die grünen, unter 20 % zusätzlich orange, unter 10 % auch rot (blinkt)
//    0 = Zeiger: 2 LEDs an der Position des Werts
//    1 = Füllbalken ab Rot: Segmente 1 bis Position
//  Sonderanzeigen: Sensorfehler = Segment 1 und 10 abwechselnd,
//  Tank leer = Segment 1 blinkt schnell, "identify" = 5 s Lauflicht.
// =============================================================================
#pragma once
#include <Arduino.h>

void displayBegin();                   // Pins einrichten, Bar aus
void displayUpdate();                  // in JEDEM loop()-Durchlauf aufrufen
void displaySetSoil(float pct, bool ok);
void displaySetTankEmpty(bool empty);
void displayIdentify();                // 5 s Lauflicht ("Welcher Kübel ist das?")
void displayLedTest();                 // Testanimation Grün -> Rot (ca. 3 s, läuft im Hintergrund)
void displayFlip();                    // Richtung der LED-Bar umdrehen (bis Neustart)
void displayTestSegments(int n);       // genau n Segmente (ab Segment 1 = rot) 10 s lang anzeigen
void displaySwapPins();                // Takt/Daten-Pin tauschen (bis Neustart) + Test

void buzzerBeep(uint8_t count);        // kurze Pieptöne (nicht, wenn stumm geschaltet)
void buzzerBeepForced(uint8_t count);  // piept auch, wenn stumm geschaltet (Test "beep")
// problems = Bitmaske ALARM_* (garden_config.h). Piept bei einem NEUEN Problem
// sofort, bei einem bestehenden höchstens alle ALARM_REPEAT_MS.
void buzzerAlarm(uint8_t problems);

void statusLedSet(uint8_t mode);       // 0 = aus, 1 = an, 2 = langsam blinken, 3 = schnell blinken
