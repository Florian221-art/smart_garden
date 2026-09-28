// garden_net.h – WLAN + HTTP(S)-POST an den Server (API-Vertrag v1.2)
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

void netBegin();
void netUpdate();       // WLAN-Reconnect, Status-LED
bool netConnected();
void netMarkError();   // Onboard-LED 5 s schnell blinken lassen
// sendet body an /api/v1/readings; bei HTTP 200 steht die Antwort in response
int netPostReading(const JsonDocument &body, JsonDocument &response);
