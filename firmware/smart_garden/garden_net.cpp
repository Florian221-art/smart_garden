#include "garden_net.h"
#include "garden_config.h"
#include "display.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

static uint32_t lastAttempt = 0;
static bool wasConnected = false;
static uint32_t errorUntil = 0;

void netMarkError() { errorUntil = millis() + 5000; }

void netBegin() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(CFG_WIFI_SSID, CFG_WIFI_PASSWORD);
  lastAttempt = millis();
  Serial.printf("[WLAN] verbinde mit \"%s\" ...\n", CFG_WIFI_SSID);
}

bool netConnected() { return WiFi.status() == WL_CONNECTED; }

void netUpdate() {
  bool c = netConnected();
  if (c && !wasConnected) {
    Serial.printf("[WLAN] verbunden, IP %s, RSSI %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  }
  if (!c && wasConnected) Serial.println("[WLAN] Verbindung verloren");
  wasConnected = c;
  if (!c) statusLedSet(2);
  else statusLedSet((int32_t)(errorUntil - millis()) > 0 ? 3 : 1);
  if (!c && millis() - lastAttempt > WIFI_RETRY_MS) {
    lastAttempt = millis();
    WiFi.disconnect();
    WiFi.begin(CFG_WIFI_SSID, CFG_WIFI_PASSWORD);
  }
}

int netPostReading(const JsonDocument &body, JsonDocument &response) {
  if (!netConnected()) return -1;

  String url = String(CFG_SERVER_URL) + "/api/v1/readings";
  String payload;
  serializeJson(body, payload);

  HTTPClient http;
  WiFiClient plain;
  WiFiClientSecure secure;
  bool ok;
  if (url.startsWith("https://")) {
#ifdef SERVER_CA_CERT
    secure.setCACert(SERVER_CA_CERT);  // Certificate Pinning (Phase 3)
#else
    secure.setInsecure();  // Übergang: verschlüsselt, aber Zertifikat ungeprüft
#endif
    ok = http.begin(secure, url);
  } else {
    ok = http.begin(plain, url);
  }
  if (!ok) return -2;

  http.setTimeout(HTTP_TIMEOUT_MS);
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-API-Key", CFG_API_KEY);

  int code = http.POST(payload);
  if (code == 200) {
    DeserializationError err = deserializeJson(response, http.getStream());
    if (err) {
      Serial.printf("[HTTP] Antwort kein gültiges JSON: %s\n", err.c_str());
      code = -3;
    }
  } else if (code > 0) {
    Serial.printf("[HTTP] Server antwortet %d: %s\n", code, http.getString().c_str());
  } else {
    Serial.printf("[HTTP] Fehler: %s\n", http.errorToString(code).c_str());
  }
  http.end();
  return code;
}
