// Beschriftungen fuer den Demo-Modus (von DemoPanel und dem Banner im Dashboard genutzt)
import type { TFunction } from 'i18next'
import { decimalSeparator } from './format'
import { describeError } from './errorMessages'
import type { DemoState, OverrideField } from './types'

export const FIELDS: { key: OverrideField; unit: string; min: number; max: number; step: number; start: number }[] = [
  { key: 'soil_moisture_pct', unit: '%', min: 0, max: 100, step: 1, start: 12 },
  { key: 'light_pct', unit: '%', min: 0, max: 100, step: 1, start: 2 },
  { key: 'air_temp_c', unit: '°C', min: -20, max: 60, step: 0.5, start: 38 },
  { key: 'air_humidity_pct', unit: '%', min: 0, max: 100, step: 1, start: 25 },
  { key: 'water_level_pct', unit: '%', min: 0, max: 100, step: 1, start: 4 },
]

const FIELD_LABEL_KEY: Record<OverrideField, string> = {
  soil_moisture_pct: 'demo.fields.soilMoisture',
  light_pct: 'demo.fields.light',
  air_temp_c: 'demo.fields.temperature',
  air_humidity_pct: 'demo.fields.humidity',
  water_level_pct: 'demo.fields.waterTank',
}

export function fieldLabel(t: TFunction, key: OverrideField): string {
  return t(FIELD_LABEL_KEY[key])
}

const FIELD_UNIT: Record<string, string> = Object.fromEntries(FIELDS.map((f) => [f.key, f.unit]))

/** z. B. "Bodenfeuchte 12 % · Temperatur-/Luftfeuchtesensor (DHT11) konnte nicht gelesen werden" */
export function describeDemo(t: TFunction, lang: string, d: Pick<DemoState, 'overrides' | 'force_errors'>): string {
  const sep = decimalSeparator(lang)
  const parts = Object.entries(d.overrides).map(([k, v]) => {
    const label = FIELD_LABEL_KEY[k as OverrideField] ? t(FIELD_LABEL_KEY[k as OverrideField]) : k
    const num = String(v).replace('.', sep)
    return `${label} ${num} ${FIELD_UNIT[k] ?? ''}`.trim()
  })
  for (const e of d.force_errors) parts.push(describeError(t, e))
  return parts.join(' · ')
}
