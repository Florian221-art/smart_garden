// =============================================================================
//  sensors.h – Bodenfeuchte (kapazitiv), Licht (LDR), Temperatur/Luftfeuchte (DHT11)
// =============================================================================
//  sensorsRead() liefert eine komplette Messung. Jeder Sensor hat ein *_ok-Flag;
//  ist es false, wird der Wert nicht verwendet (keine Auto-Bewässerung bei
//  defektem Bodensensor) und an den Server als null + Fehlercode gemeldet.
// =============================================================================
#pragma once
#include <Arduino.h>

struct Readings {
  bool soil_ok = false;   // Bodensensor plausibel UND Kalibrierung gültig
  float soil_pct = 0;     // 0 = trocken, 100 = nass (nach Kalibrierung)
  int soil_raw = 0;       // ADC-Rohwert 0-4095 (immer echt, auch im Demo-Modus)

  bool light_ok = false;
  float light_pct = 0;    // 0 = dunkel, 100 = hell
  int light_raw = 0;

  bool dht_ok = false;    // DHT11 hat in den letzten 30 s einen gültigen Wert geliefert
  float temp_c = 0;
  float hum_pct = 0;

  // gesetzt, wenn ein Wert vom Demo-Modus stammt (nur für die serielle Anzeige)
  bool demo_soil = false, demo_light = false, demo_temp = false, demo_hum = false;
};

void sensorsBegin();
Readings sensorsRead();       // echte Messung (dauert ca. 30 ms, DHT11 höchstens alle 5 s)
int sensorsReadSoilRaw();     // Median aus 5 Messungen – auch für die Kalibrierung
int sensorsReadLightRaw();
