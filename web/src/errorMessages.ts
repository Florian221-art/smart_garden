// Uebersetzt die Fehlercodes aus der ESP-Firmware (firmware/smart_garden/smart_garden.ino)
// in verstaendlichen Text (message_key-Prinzip, siehe api-contract.md Abschnitt 5).
// Server/ESP schicken nur den Code, die Uebersetzung kommt aus i18n/locales/*.json.
import type { TFunction } from 'i18next'

const KNOWN_CODES = ['soil_out_of_range', 'light_read_failed', 'dht_read_failed', 'tank_empty'] as const

export function describeError(t: TFunction, code: string): string {
  if ((KNOWN_CODES as readonly string[]).includes(code)) return t(`errorsCode.${code}`)
  return code
}
