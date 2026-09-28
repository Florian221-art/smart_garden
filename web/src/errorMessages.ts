// Uebersetzt die Fehlercodes aus der ESP-Firmware (firmware/smart_garden/smart_garden.ino)
// in verstaendlichen Text. Server/ESP schicken nur den Code (message_key-Prinzip,
// siehe api-contract.md Abschnitt 5) - echte Mehrsprachigkeit (NL/DE/EN via
// react-i18next) kommt in Phase 2, hier erstmal fest auf Deutsch.
const ERROR_MESSAGES: Record<string, string> = {
  soil_out_of_range: 'Bodensensor außerhalb des Messbereichs',
  light_read_failed: 'Lichtsensor konnte nicht gelesen werden',
  dht_read_failed: 'Temperatur-/Luftfeuchtesensor (DHT11) konnte nicht gelesen werden',
  tank_empty: 'Wassertank leer – automatische Bewässerung gestoppt',
}

export function describeError(code: string): string {
  return ERROR_MESSAGES[code] ?? code
}
