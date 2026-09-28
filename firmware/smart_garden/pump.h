// pump.h – Relais/Pumpe mit harten Sicherheitsgrenzen (nicht blockierend)
#pragma once
#include <Arduino.h>

void pumpBegin();
// startet die Pumpe; gibt false zurück, wenn eine Sicherheitsgrenze greift
bool pumpStart(float seconds, const char *reason, bool respectCooldown);
void pumpStop();
void pumpUpdate();              // in loop() aufrufen
bool pumpRunning();
bool pumpCooldownActive();
uint32_t pumpCooldownLeftS();
float pumpTakeOnSecondsSinceLast(); // liefert Laufzeit seit letztem erfolgreichen POST
void pumpRestoreOnSeconds(float s); // falls POST fehlschlägt: zurückbuchen
float pumpTodaySeconds();
