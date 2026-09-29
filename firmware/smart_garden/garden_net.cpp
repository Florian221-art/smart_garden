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

// WLAN-Verbindungsversuche (siehe netUpdate):
// Ein neuer Versuch wird erst gestartet, wenn der vorige nachweislich beendet ist
// (Ereignis "getrennt" vom ESP32-Core). So ruft die Firmware nie esp_wifi_connect()
// auf, während noch ein Versuch läuft -> keine Meldungen
// "wifi:sta is connecting, cannot set config / return error" mehr.
static bool wifiEnabled = false;             // false = Platzhalter-Passwort -> WLAN bleibt aus
static uint32_t lastAttempt = 0;             // Start des letzten Versuchs
static volatile bool attemptDone = false;    // letzter Versuch beendet (Ereignis kam)
static volatile uint32_t lastDiscMs = 0;     // Zeitpunkt des letzten "getrennt"-Ereignisses
static volatile uint8_t lastReason = 0;      // Grund laut ESP-IDF (wifi_err_reason_t)
static uint8_t failedAttempts = 0;           // gescheiterte Versuche in Folge
static bool wasConnected = false;
static uint32_t errorSince = 0;   // Zeitpunkt des letzten Sendefehlers
static bool errorShown = false;   // Fehler-Blinken aktiv (Flag statt Zeitstempel 0 -> überlaufsicher)

void netMarkError() {
  errorSince = millis();
  errorShown = true;
}

// Läuft im WLAN-Task des Cores -> nur Variablen setzen, keine Ausgabe hier.
static void onWifiDisconnected(arduino_event_id_t, arduino_event_info_t info) {
  // Eigenes Trennen (WLAN-Neustart) ist kein beendeter Versuch -> ignorieren
  if (info.wifi_sta_disconnected.reason == WIFI_REASON_ASSOC_LEAVE) return;
  lastReason = info.wifi_sta_disconnected.reason;
  lastDiscMs = millis();
  attemptDone = true;
}

// Übersetzt den Trenngrund in einen verständlichen Hinweis
static const char *reasonText(uint8_t r) {
  switch (r) {
    case WIFI_REASON_NO_AP_FOUND:
      return "nicht gefunden (Pi-Hotspot aus oder zu weit weg)";
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_802_1X_AUTH_FAILED:
    case WIFI_REASON_AUTH_EXPIRE:
      return "gefunden, Anmeldung gescheitert -> Passwort prüfen ODER Signal schwach (ESP näher an den Pi)";
    case WIFI_REASON_BEACON_TIMEOUT:
      return "Verbindung abgerissen (Signal zu schwach?)";
    default:
      return nullptr;  // anderer Grund -> Nummer ausgeben
  }
}

void netBegin() {
  // Mit dem Platzhalter-Passwort kann die Verbindung nie klappen. WLAN dann gar
  // nicht erst starten: spart Strom und vermeidet Fehlermeldungen im Log.
  if (strcmp(CFG_WIFI_PASSWORD, PLACEHOLDER_WIFI_PASSWORD) == 0) {
    Serial.println("[WLAN] AUS: WLAN-Passwort ist noch der Platzhalter \"" PLACEHOLDER_WIFI_PASSWORD "\".");
    Serial.println("[WLAN] Echtes Passwort in secrets.h (oder oben im Sketch) eintragen und neu hochladen.");
    WiFi.mode(WIFI_OFF);
    return;
  }
  wifiEnabled = true;
  WiFi.persistent(false);       // Zugangsdaten NICHT zusätzlich im WLAN-Flashbereich ablegen
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false); // Neuverbindung macht netUpdate() selbst (kontrolliert, s. o.)
  WiFi.setSleep(false);         // kein Modem-Sleep: stabilerer Handshake mit dem Pi-Hotspot
  WiFi.onEvent(onWifiDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
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
  if (c) failedAttempts = 0;
  if (!wifiEnabled || c) return;

  // Grund des letzten Fehlschlags einmal verständlich melden (nur bei Änderung)
  static uint8_t reportedReason = 0;
  uint8_t r = lastReason;
  if (r && r != reportedReason && r != WIFI_REASON_ASSOC_LEAVE) {
    reportedReason = r;
    const char *txt = reasonText(r);
    if (txt) Serial.printf("[WLAN] \"%s\" %s (Grund %u)\n", CFG_WIFI_SSID, txt, (unsigned)r);
    else Serial.printf("[WLAN] Verbindung fehlgeschlagen (Grund %u), neuer Versuch folgt\n", (unsigned)r);
  }

  // Neuer Versuch erst, wenn der vorige beendet ist und WIFI_RETRY_MS Ruhe war.
  // Kommt nach 30 s kein Ereignis, gilt der Versuch als hängend und wird ersetzt.
  uint32_t now = millis();
  bool done = attemptDone && now - lastDiscMs >= WIFI_RETRY_MS;
  bool hung = !attemptDone && now - lastAttempt >= 30000;
  if (done || hung) {
    attemptDone = false;
    lastAttempt = now;
    failedAttempts++;
    if (failedAttempts % 3 == 0) {
      // Nach 3 Fehlschlägen WLAN komplett neu aufsetzen (frischer Scan + Handshake).
      // Sicher, weil der vorige Versuch beendet ist -> kein "sta is connecting".
      Serial.printf("[WLAN] %u Versuche gescheitert -> WLAN wird neu gestartet\n", (unsigned)failedAttempts);
      WiFi.disconnect(false, false);
      WiFi.begin(CFG_WIFI_SSID, CFG_WIFI_PASSWORD);
    } else {
      WiFi.reconnect();  // nutzt die Daten aus netBegin(), kein neues "set config"
    }
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
