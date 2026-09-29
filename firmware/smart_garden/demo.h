// =============================================================================
//  demo.h – Demo-Modus: Sensorwerte überschreiben, Sensorausfälle simulieren
// =============================================================================
//  Zweck: In der Präsentation Situationen zeigen, die man nicht abwarten kann
//  (trockene Erde, Hitze, leerer Tank, Sensorausfall). API-Vertrag Abschnitt 4.
//
//  Steuerung:
//    - vom Server: Objekt "demo" in jeder Antwort ({expires_in_s, overrides, force_errors})
//    - seriell:    "demo soil 12 [s]", "demo error dht [s]", "demo off"
//
//  Regeln:
//    - Demo-Werte gelten höchstens MAX_DEMO_S (600 s), danach automatisch echte Werte.
//    - Jede Nachricht meldet in "demo_overrides", welche Felder nicht echt sind.
//    - Rohwerte (*_raw) bleiben immer echt.
//    - Alle Sicherheitsgrenzen der Pumpe gelten auch im Demo-Modus. Ein
//      Demo-Füllstand kann den Tank nur leerer, nie voller machen (tank.h).
// =============================================================================
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "sensors.h"

void demoApplyFromServer(JsonVariantConst demo);                        // Feld "demo" aus der Serverantwort
void demoSetValue(const String &field, float value, uint32_t seconds);  // seriell: demo <feld> <wert> [s]
void demoForceError(const String &err, uint32_t seconds);               // seriell: demo error <sensor> [s]
void demoOff();                                                         // sofort zurück zu echten Werten
bool demoActive();                                                      // prüft auch den Ablauf-Timer
void demoApply(Readings &r);                  // überschreibt Werte in r, setzt Fehler, Demo-Tank
void demoOverriddenFields(JsonArray fields);  // Feldnamen für "demo_overrides"
void demoPrint();                             // Status seriell ausgeben
bool demoTakeSoilKick();                      // true (einmalig) nach neuer Bodenfeuchte-Vorgabe -> sofort gießen
