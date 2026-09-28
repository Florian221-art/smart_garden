// demo.h – Demo-Modus: Sensorwerte überschreiben, Fehler erzwingen (API-Vertrag Abschnitt 4)
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "sensors.h"

void demoApplyFromServer(JsonVariantConst demo);   // Feld "demo" aus der Serverantwort
void demoSetValue(const String &field, float value, uint32_t seconds); // seriell
void demoForceError(const String &err, uint32_t seconds);             // seriell
void demoOff();
bool demoActive();
void demoApply(Readings &r);                 // überschreibt Werte in r, setzt Fehler
void demoForcedErrors(JsonArray errors);     // erzwungene Fehlercodes anhängen
void demoOverriddenFields(JsonArray fields); // für "demo_overrides"
void demoPrint();
