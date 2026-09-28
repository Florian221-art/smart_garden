// =============================================================================
//  garden_net.h – WLAN-Verbindung und HTTP(S)-POST an den Server (API-Vertrag v1.2)
// =============================================================================
//  - Das WLAN verbindet sich im Hintergrund neu, falls es abbricht.
//  - Senden: POST {CFG_SERVER_URL}/api/v1/readings mit Header X-API-Key.
//  - https:// wird unterstützt. Mit SERVER_CA_CERT (in secrets.h) wird das
//    Server-Zertifikat geprüft, ohne ist die Verbindung zwar verschlüsselt,
//    aber nicht gegen einen gefälschten Server geschützt (Warnung im Log).
//  - Die Firmware funktioniert auch komplett ohne WLAN/Server weiter
//    (Messen, LED-Bar, Auto-Bewässerung, Demo per Konsole).
// =============================================================================
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

void netBegin();        // WLAN starten (nicht blockierend)
void netUpdate();       // in loop(): Reconnect, Status-LED
bool netConnected();
void netMarkError();    // Onboard-LED 5 s schnell blinken lassen
// Sendet body an /api/v1/readings. Rückgabe: HTTP-Status, oder negativ bei
// Verbindungsfehler (-1 kein WLAN, -2 URL ungültig, -3 Antwort kein JSON).
// Nur bei 200 steht die geparste Antwort in response.
int netPostReading(const JsonDocument &body, JsonDocument &response);
