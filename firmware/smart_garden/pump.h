// =============================================================================
//  pump.h – Relais/Pumpe mit harten Sicherheitsgrenzen (nicht blockierend)
// =============================================================================
//  Jeder Pumpenlauf – automatisch, Demo, Dashboard oder seriell – läuft durch
//  pumpStart() und damit durch dieselben Prüfungen:
//    1. Tank laut Schätzung nicht leer        (Trockenlaufschutz)
//    2. Mindestpause HARD_MIN_COOLDOWN_S      (immer) bzw. pump_cooldown_s (Auto)
//    3. Laufzeit <= max_pump_s_per_run <= HARD_MAX_PUMP_S_PER_RUN
//    4. Tageslimit max_pump_s_per_day <= HARD_MAX_PUMP_S_PER_DAY
//  Ausgeschaltet wird in pumpUpdate() über millis() – ohne delay(). Hängt die
//  Firmware trotzdem, startet der Watchdog den ESP neu und pumpBegin() schaltet
//  das Relais als Erstes AUS.
// =============================================================================
#pragma once
#include <Arduino.h>

void pumpBegin();                    // Relais sicher AUS – als ALLERERSTES in setup() aufrufen
// Startet die Pumpe für höchstens `seconds` Sekunden.
// respectCooldown = true: die (längere) Pause aus den Settings gilt (Auto-Bewässerung).
// Gibt false zurück, wenn eine Sicherheitsgrenze greift (Grund steht im seriellen Log).
bool pumpStart(float seconds, const char *reason, bool respectCooldown);
void pumpStop();                     // sofort aus
void pumpUpdate();                   // in JEDEM loop()-Durchlauf aufrufen
bool pumpRunning();
bool pumpCooldownActive();           // läuft die Pause aus den Settings noch?
uint32_t pumpCooldownLeftS();        // Restzeit dieser Pause
float pumpTakeOnSecondsSinceLast();  // Laufzeit seit dem letzten erfolgreichen POST (und zurücksetzen)
void pumpRestoreOnSeconds(float s);  // POST fehlgeschlagen: Laufzeit fürs nächste Mal zurückbuchen
float pumpTodaySeconds();            // Laufzeit im aktuellen 24-h-Fenster
