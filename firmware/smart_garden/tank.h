// =============================================================================
//  tank.h – geschätzter Füllstand des Wassertanks (es gibt keinen Sensor)
// =============================================================================
//  Prinzip: Restmenge -= Pumpenlaufzeit × Durchfluss (settings.pump_flow_ml_per_s).
//  Die Restmenge liegt im Flash und übersteht Neustarts. Nach dem Auffüllen
//  muss der Tank als voll markiert werden ("refill", BOOT-Taste 3 s oder
//  Dashboard-Button "Tank aufgefüllt").
//
//  Demo-Modus: Ein Demo-Füllstand wird nur ANGEZEIGT/GEMELDET und nicht
//  gespeichert. Für den Trockenlaufschutz zählt immer der kleinere Wert aus
//  echter Schätzung und Demo-Wert – eine Demo kann einen leeren Tank also
//  vortäuschen, aber nie einen echten leeren Tank verstecken.
// =============================================================================
#pragma once
#include <Arduino.h>

void tankBegin();                      // Restmenge aus dem Flash laden (erster Start: voll)
void tankConsume(float pump_seconds);  // nach jedem Pumpenlauf abziehen
void tankRefill(const char *source);   // Tank voll (Quelle wird geloggt)
void tankSetDemoPct(float pct);        // Demo-Füllstand setzen, < 0 = keine Demo
float tankRemainingMl();               // gemeldete Restmenge (ggf. Demo-Wert)
float tankLevelPct();                  // gemeldeter Füllstand in % (ggf. Demo-Wert)
bool tankIsEmpty();                    // Trockenlaufschutz: min(echt, Demo) <= TANK_EMPTY_PCT
bool tankIsLow();                      // gemeldeter Füllstand <= settings.tank_low_pct
