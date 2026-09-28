// =============================================================================
//  sensors.cpp – Sensoren auslesen (siehe sensors.h)
// =============================================================================
#include "sensors.h"
#include "garden_config.h"
#include <DHT.h>

static DHT dht(PIN_DHT, DHT11);

// Plausibilitätsgrenzen des Bodensensors (Rohwert). Außerhalb davon ist der
// Sensor abgezogen, kurzgeschlossen oder falsch verkabelt -> als Fehler melden,
// statt 0 % bzw. 100 % anzunehmen (0 % würde sonst Dauergießen auslösen).
static const int SOIL_RAW_MIN_VALID = 100;
static const int SOIL_RAW_MAX_VALID = 4000;

// 5 Messungen, mittlerer Wert (Median) – filtert einzelne Ausreißer des ESP32-ADC
static int medianAnalog(uint8_t pin) {
  int v[5];
  for (int i = 0; i < 5; i++) {
    v[i] = analogRead(pin);
    delay(3);
  }
  for (int i = 1; i < 5; i++) {  // Insertion-Sort für 5 Werte
    int k = v[i], j = i - 1;
    while (j >= 0 && v[j] > k) {
      v[j + 1] = v[j];
      j--;
    }
    v[j + 1] = k;
  }
  return v[2];
}

// Rohwert linear auf 0-100 % abbilden. raw0 = Rohwert bei 0 %, raw100 bei 100 %
// (funktioniert auch, wenn raw0 > raw100 ist, wie bei beiden Sensoren hier).
static float mapPct(int raw, int raw0, int raw100) {
  if (raw0 == raw100) return 0;
  float pct = (float)(raw - raw0) * 100.0f / (float)(raw100 - raw0);
  return constrain(pct, 0.0f, 100.0f);
}

void sensorsBegin() {
  analogReadResolution(12);                     // 0-4095
  analogSetPinAttenuation(PIN_SOIL, ADC_11db);  // Messbereich bis ca. 3,1 V
  analogSetPinAttenuation(PIN_LIGHT, ADC_11db);
  dht.begin();
}

int sensorsReadSoilRaw() { return medianAnalog(PIN_SOIL); }
int sensorsReadLightRaw() { return medianAnalog(PIN_LIGHT); }

Readings sensorsRead() {
  Readings r;

  // Kapazitiver Sensor: trocken = hoher Wert, nass = niedriger Wert
  r.soil_raw = sensorsReadSoilRaw();
  r.soil_ok = r.soil_raw > SOIL_RAW_MIN_VALID && r.soil_raw < SOIL_RAW_MAX_VALID && configSoilCalibrationValid();
  r.soil_pct = mapPct(r.soil_raw, calib.soil_raw_dry, calib.soil_raw_wet);

  // LDR-Modul: dunkel = hoher Wert, hell = niedriger Wert. Beide Extremwerte
  // sind real möglich, daher keine Plausibilitätsprüfung.
  r.light_raw = sensorsReadLightRaw();
  r.light_ok = true;
  r.light_pct = mapPct(r.light_raw, calib.light_raw_dark, calib.light_raw_bright);

  // DHT11: höchstens alle 5 s lesen (öfter liefert er Fehler). Den letzten guten
  // Wert bis zu 30 s weiterverwenden – einzelne Aussetzer sind beim DHT11 normal.
  static uint32_t lastDhtRead = 0, lastDhtOk = 0;
  static float lastT = NAN, lastH = NAN;
  uint32_t now = millis();
  if (lastDhtRead == 0 || now - lastDhtRead >= 5000) {
    lastDhtRead = now;
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    // DHT11-Messbereich: 0-50 °C, 20-90 % – großzügig prüfen, Unsinn verwerfen
    if (!isnan(t) && !isnan(h) && t > -20 && t < 80 && h >= 0 && h <= 100) {
      lastT = t;
      lastH = h;
      lastDhtOk = now;
    }
  }
  r.dht_ok = !isnan(lastT) && (now - lastDhtOk) < 30000;
  r.temp_c = r.dht_ok ? lastT : 0;
  r.hum_pct = r.dht_ok ? lastH : 0;
  return r;
}
