// sensors.h – Bodenfeuchte (kapazitiv), Licht (LDR), Temperatur/Luftfeuchte (DHT11)
#pragma once
#include <Arduino.h>

struct Readings {
  bool soil_ok = false;
  float soil_pct = 0;
  int soil_raw = 0;

  bool light_ok = false;
  float light_pct = 0;
  int light_raw = 0;

  bool dht_ok = false;
  float temp_c = 0;
  float hum_pct = 0;

  // gesetzt, wenn ein Wert vom Demo-Modus stammt
  bool demo_soil = false, demo_light = false, demo_temp = false, demo_hum = false;
};

void sensorsBegin();
Readings sensorsRead();       // echte Messung
int sensorsReadSoilRaw();     // für Kalibrierung
int sensorsReadLightRaw();
