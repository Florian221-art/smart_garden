#include "sensors.h"
#include "garden_config.h"
#include <DHT.h>

static DHT dht(PIN_DHT, DHT11);

static int medianAnalog(uint8_t pin) {
  int v[5];
  for (int i = 0; i < 5; i++) {
    v[i] = analogRead(pin);
    delay(3);
  }
  // Insertion-Sort für 5 Werte
  for (int i = 1; i < 5; i++) {
    int k = v[i], j = i - 1;
    while (j >= 0 && v[j] > k) { v[j + 1] = v[j]; j--; }
    v[j + 1] = k;
  }
  return v[2];
}

static float mapPct(int raw, int raw0, int raw100) {
  if (raw0 == raw100) return 0;
  float pct = (float)(raw - raw0) * 100.0f / (float)(raw100 - raw0);
  return constrain(pct, 0.0f, 100.0f);
}

void sensorsBegin() {
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_SOIL, ADC_11db);   // Messbereich bis ~3,1 V
  analogSetPinAttenuation(PIN_LIGHT, ADC_11db);
  dht.begin();
}

int sensorsReadSoilRaw() { return medianAnalog(PIN_SOIL); }
int sensorsReadLightRaw() { return medianAnalog(PIN_LIGHT); }

Readings sensorsRead() {
  Readings r;

  // Kapazitiver Sensor: trocken = hoher Wert, nass = niedriger Wert
  r.soil_raw = sensorsReadSoilRaw();
  // < 100: Sensor nicht angeschlossen / Kurzschluss
  r.soil_ok = r.soil_raw > 100;
  r.soil_pct = mapPct(r.soil_raw, calib.soil_raw_dry, calib.soil_raw_wet);

  // LDR-Modul: dunkel = hoher Wert, hell = niedriger Wert
  r.light_raw = sensorsReadLightRaw();
  r.light_ok = true;
  r.light_pct = mapPct(r.light_raw, calib.light_raw_dark, calib.light_raw_bright);

  // DHT11: höchstens alle 5 s lesen (sonst liefert er Fehler), letzten guten Wert
  // bis zu 30 s weiterverwenden, erst danach als Ausfall melden.
  static uint32_t lastDhtRead = 0, lastDhtOk = 0;
  static float lastT = NAN, lastH = NAN;
  uint32_t now = millis();
  if (lastDhtRead == 0 || now - lastDhtRead >= 5000) {
    lastDhtRead = now;
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t) && !isnan(h)) {
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
