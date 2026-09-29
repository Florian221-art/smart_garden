// Uebersetzt Messwerte in verstaendliche Aussagen ("Erde ist trocken") fuer das Dashboard.
// Grenzwerte passend zur Default-Config aus api-contract.md (Giessschwelle 30 %, Tank-Warnung 20 %).
// Alle Texte kommen aus i18n (t) statt fest verdrahtet zu sein - siehe src/i18n/locales/*.json.
import type { TFunction } from 'i18next'
import type { LatestReading } from './types'
import type { Tone } from './components/ui'
import { describeError } from './errorMessages'

/** Nach dieser Zeit ohne Meldung gilt das Geraet als offline (Sendeintervall 15 s -> 6 Meldungen verpasst) */
export const OFFLINE_AFTER_S = 90

export interface Assessment {
  tone: Tone
  text: string
}

const SOIL_DRY = 30
const SOIL_VERY_DRY = 20
const SOIL_WET = 80
const TANK_LOW = 20
const TANK_EMPTY = 5

export function soilStatus(t: TFunction, pct: number | null): Assessment {
  if (pct === null) return { tone: 'danger', text: t('status.soil.sensorMissing') }
  if (pct < SOIL_VERY_DRY) return { tone: 'danger', text: t('status.soil.veryDry') }
  if (pct < SOIL_DRY) return { tone: 'warn', text: t('status.soil.dry') }
  if (pct > SOIL_WET) return { tone: 'warn', text: t('status.soil.wet') }
  return { tone: 'ok', text: t('status.soil.ok') }
}

export function tankStatus(t: TFunction, pct: number): Assessment {
  if (pct <= TANK_EMPTY) return { tone: 'danger', text: t('status.tank.empty') }
  if (pct < TANK_LOW) return { tone: 'warn', text: t('status.tank.low') }
  return { tone: 'ok', text: t('status.tank.ok') }
}

export function tempStatus(t: TFunction, c: number | null): Assessment {
  if (c === null) return { tone: 'danger', text: t('status.temp.sensorMissing') }
  if (c >= 35) return { tone: 'warn', text: t('status.temp.hot') }
  if (c <= 5) return { tone: 'warn', text: t('status.temp.cold') }
  return { tone: 'ok', text: t('status.temp.ok') }
}

export function humidityStatus(t: TFunction, pct: number | null): Assessment {
  if (pct === null) return { tone: 'danger', text: t('status.humidity.sensorMissing') }
  if (pct < 30) return { tone: 'neutral', text: t('status.humidity.dry') }
  if (pct > 80) return { tone: 'neutral', text: t('status.humidity.humid') }
  return { tone: 'ok', text: t('status.humidity.ok') }
}

export function lightStatus(t: TFunction, pct: number | null): Assessment {
  if (pct === null) return { tone: 'danger', text: t('status.light.sensorMissing') }
  if (pct < 10) return { tone: 'neutral', text: t('status.light.dark') }
  if (pct < 40) return { tone: 'neutral', text: t('status.light.dim') }
  return { tone: 'ok', text: t('status.light.bright') }
}

export function signalLabel(t: TFunction, rssi: number): { text: string; bars: 0 | 1 | 2 | 3 } {
  if (rssi >= -60) return { text: t('status.signal.veryGood'), bars: 3 }
  if (rssi >= -70) return { text: t('status.signal.good'), bars: 2 }
  if (rssi >= -80) return { text: t('status.signal.weak'), bars: 1 }
  return { text: t('status.signal.veryWeak'), bars: 0 }
}

export function secondsSince(iso: string, now: number): number {
  return Math.max(0, Math.round((now - new Date(iso).getTime()) / 1000))
}

/** "gerade eben", "vor 12 s", "vor 3 min", "vor 2 h" */
export function relativeTime(t: TFunction, seconds: number): string {
  if (seconds < 5) return t('time.justNow')
  if (seconds < 60) return t('time.secondsAgo', { s: seconds })
  if (seconds < 3600) return t('time.minutesAgo', { m: Math.floor(seconds / 60) })
  if (seconds < 86400) return t('time.hoursAgo', { h: Math.floor(seconds / 3600) })
  return t('time.daysAgo', { d: Math.floor(seconds / 86400) })
}

export function fmtDuration(t: TFunction, s: number): string {
  if (s < 60) return t('duration.seconds', { s })
  if (s < 3600) return t('duration.minutes', { m: Math.floor(s / 60) })
  if (s < 86400) return t('duration.hoursMinutes', { h: Math.floor(s / 3600), m: Math.floor((s % 3600) / 60) })
  return t('duration.daysHours', { d: Math.floor(s / 86400), h: Math.floor((s % 86400) / 3600) })
}

export interface Overall {
  tone: Tone
  title: string
  /** Einzelne Punkte, die Aufmerksamkeit brauchen (leer = alles gut) */
  issues: Assessment[]
}

/** Gesamtzustand fuer die grosse Statuskarte oben */
export function overallStatus(t: TFunction, r: LatestReading | null, offline: boolean): Overall {
  if (!r) return { tone: 'neutral', title: t('hero.waitingFirst'), issues: [] }
  if (offline)
    return {
      tone: 'danger',
      title: t('hero.offlineTitle'),
      issues: [{ tone: 'danger', text: t('hero.offlineIssue') }],
    }
  const issues: Assessment[] = []
  const soil = soilStatus(t, r.soil_moisture_pct)
  if (r.soil_moisture_pct !== null && soil.tone !== 'ok')
    issues.push({ tone: soil.tone, text: t('hero.issueSoil', { text: soil.text }) })
  const tank = tankStatus(t, r.water_level_pct)
  if (tank.tone !== 'ok') issues.push({ tone: tank.tone, text: t('hero.issueTank', { text: tank.text }) })
  const temp = tempStatus(t, r.air_temp_c)
  if (r.air_temp_c !== null && temp.tone !== 'ok')
    issues.push({ tone: temp.tone, text: t('hero.issueTemp', { text: temp.text }) })
  for (const e of r.errors) {
    if (e === 'tank_empty') continue // steht schon beim Tank
    issues.push({ tone: 'danger', text: describeError(t, e) })
  }
  if (issues.length === 0) return { tone: 'ok', title: t('hero.allGoodTitle'), issues }
  const worst: Tone = issues.some((i) => i.tone === 'danger') ? 'danger' : 'warn'
  return {
    tone: worst,
    title: worst === 'danger' ? t('hero.needsHelpTitle') : t('hero.checkTitle'),
    issues,
  }
}
