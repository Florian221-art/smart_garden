// =============================================================================
//  garden_net.cpp – WLAN + HTTP(S) (siehe garden_net.h)
// =============================================================================
#include "garden_net.h"
#include "garden_config.h"
#include "display.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

// secrets.h kann das Root-Zertifikat des Servers (Caddy "tls internal") als
// #define SERVER_CA_CERT "-----BEGIN CERTIFICATE-----\n..." enthalten.
#if __has_include("secrets.h")
#include "secrets.h"
#endif

static uint32_t lastAttempt = 0;
static bool wasConnected = false;
static uint32_t errorSince = 0;   // Zeitpunkt des letzten Sendefehlers
static bool errorShown = false;   // Fehler-Blinken aktiv (Flag statt Zeitstempel 0 -> überlaufsicher)

void netMarkError() {
  errorSince = millis();
  errorShown = true;
}

void netBegin() {
  WiFi.persistent(false);  // Zugangsdaten NICHT zusätzlich im WLAN-Flashbereich ablegen
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(CFG_WIFI_SSID, CFG_WIFI_PASSWORD);
  lastAttempt = millis();
  Serial.printf("[WLAN] verbinde mit \"%s\" ...\n", CFG_WIFI_SSID);
#ifndef SERVER_CA_CERT
  if (strncmp(CFG_SERVER_URL, "https://", 8) == 0)
    Serial.println("[WLAN] Hinweis: HTTPS ohne SERVER_CA_CERT -> verschlüsselt, Server-Zertifikat aber ungeprüft");
#endif
}

bool netConnected() { return WiFi.status() == WL_CONNECTED; }

void netUpdate() {
  bool c = netConnected();
  if (c && !wasConnected)
    Serial.printf("[WLAN] verbunden, IP %s, RSSI %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  if (!c && wasConnected) Serial.println("[WLAN] Verbindung verloren");
  wasConnected = c;
  if (!c) statusLedSet(2);
  else {
    if (errorShown && millis() - errorSince >= 5000) errorShown = false;
    statusLedSet(errorShown ? 3 : 1);
  }
  // Kein WLAN: höchstens alle WIFI_RETRY_MS einen neuen Versuch anstoßen – aber nur,
  // wenn der ESP gerade NICHT selbst verbindet. (Früher wurde jedes Mal WiFi.begin()
  // aufgerufen; lief noch ein Versuch, meldete der Core
  // "wifi:sta is connecting, cannot set config".)
  if (!c && millis() - lastAttempt > WIFI_RETRY_MS) {
    lastAttempt = millis();
    wl_status_t st = WiFi.status();
    static wl_status_t lastReported = WL_IDLE_STATUS;
    if (st != lastReported) {  // Grund einmal verständlich melden
      lastReported = st;
      if (st == WL_NO_SSID_AVAIL)
        Serial.printf("[WLAN] \"%s\" nicht gefunden (Pi-Hotspot aus oder zu weit weg)\n", CFG_WIFI_SSID);
      else if (st == WL_CONNECT_FAILED)
        Serial.println("[WLAN] Verbindung abgelehnt – WLAN-Passwort prüfen (oben im Sketch bzw. secrets.h)");
    }
    if (st == WL_NO_SSID_AVAIL || st == WL_CONNECT_FAILED || st == WL_CONNECTION_LOST || st == WL_DISCONNECTED)
      WiFi.reconnect();  // nutzt die gespeicherten Daten aus netBegin(), kein neues "set config"
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
    secure.setCACert(SERVER_CA_CERT);  // nur ein Server mit diesem Zertifikat wird akzeptiert
#else
    secure.setInsecure();              // Übergang: verschlüsselt, Zertifikat ungeprüft
#endif
    // Standard wären 120 s – deutlich länger als der Watchdog (20 s)
    secure.setHandshakeTimeout(HTTP_TIMEOUT_MS / 1000);
    ok = http.begin(secure, url);
  } else {
    ok = http.begin(plain, url);
  }
  if (!ok) return -2;

  http.setTimeout(HTTP_TIMEOUT_MS);
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.setReuse(false);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-API-Key", CFG_API_KEY);  // wird nie ins Log geschrieben

  int code = http.POST(payload);
  if (code == 200) {
    DeserializationError err = deserializeJson(response, http.getStream());
    if (err) {
      Serial.printf("[HTTP] Antwort kein gültiges JSON: %s\n", err.c_str());
      code = -3;
    }
  } else if (code > 0) {
    // Fehlertext des Servers gekürzt ausgeben (z. B. 401 = falscher API-Key, 422 = Feldfehler)
    String msg = http.getString();
    if (msg.length() > 200) msg = msg.substring(0, 200) + "...";
    Serial.printf("[HTTP] Server antwortet %d: %s\n", code, msg.c_str());
    if (code == 401) Serial.println("[HTTP] -> API-Key oder Geräte-ID falsch (oben im Sketch bzw. secrets.h)");
  } else {
    Serial.printf("[HTTP] Fehler: %s\n", http.errorToString(code).c_str());
  }
  http.end();
  return code;
}
