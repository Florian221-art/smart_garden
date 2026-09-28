// Beschriftungen fuer den Demo-Modus (von DemoPanel und dem Banner im Dashboard genutzt)
import { describeError } from './errorMessages'
import type { DemoState, OverrideField } from './types'

export const FIELDS: { key: OverrideField; label: string; unit: string; min: number; max: number; step: number; start: number }[] = [
  { key: 'soil_moisture_pct', label: 'Bodenfeuchte', unit: '%', min: 0, max: 100, step: 1, start: 12 },
  { key: 'light_pct', label: 'Licht', unit: '%', min: 0, max: 100, step: 1, start: 2 },
  { key: 'air_temp_c', label: 'Temperatur', unit: '°C', min: -20, max: 60, step: 0.5, start: 38 },
  { key: 'air_humidity_pct', label: 'Luftfeuchte', unit: '%', min: 0, max: 100, step: 1, start: 25 },
  { key: 'water_level_pct', label: 'Wassertank', unit: '%', min: 0, max: 100, step: 1, start: 4 },
]

const FIELD_LABEL: Record<string, string> = Object.fromEntries(FIELDS.map((f) => [f.key, f.label]))
const FIELD_UNIT: Record<string, string> = Object.fromEntries(FIELDS.map((f) => [f.key, f.unit]))

/** z. B. "Bodenfeuchte 12 % · Temperatur-/Luftfeuchtesensor (DHT11) konnte nicht gelesen werden" */
export function describeDemo(d: Pick<DemoState, 'overrides' | 'force_errors'>): string {
  const parts = Object.entries(d.overrides).map(
    ([k, v]) => `${FIELD_LABEL[k] ?? k} ${String(v).replace('.', ',')} ${FIELD_UNIT[k] ?? ''}`.trim(),
  )
  for (const e of d.force_errors) parts.push(describeError(e))
  return parts.join(' · ')
}
