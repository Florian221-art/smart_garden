// Uebersetzt Messwerte in verstaendliche Aussagen ("Erde ist trocken") fuer das Dashboard.
// Grenzwerte passend zur Default-Config aus api-contract.md (Giessschwelle 30 %, Tank-Warnung 20 %).
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

export function soilStatus(pct: number | null): Assessment {
  if (pct === null) return { tone: 'danger', text: 'Sensor liefert keinen Wert' }
  if (pct < SOIL_VERY_DRY) return { tone: 'danger', text: 'Sehr trocken' }
  if (pct < SOIL_DRY) return { tone: 'warn', text: 'Trocken – wird bald gegossen' }
  if (pct > SOIL_WET) return { tone: 'warn', text: 'Sehr nass' }
  return { tone: 'ok', text: 'Gut feucht' }
}

export function tankStatus(pct: number): Assessment {
  if (pct <= TANK_EMPTY) return { tone: 'danger', text: 'Leer – bitte auffüllen' }
  if (pct < TANK_LOW) return { tone: 'warn', text: 'Fast leer – bald auffüllen' }
  return { tone: 'ok', text: 'Genug Wasser' }
}

export function tempStatus(c: number | null): Assessment {
  if (c === null) return { tone: 'danger', text: 'Sensor liefert keinen Wert' }
  if (c >= 35) return { tone: 'warn', text: 'Sehr heiß' }
  if (c <= 5) return { tone: 'warn', text: 'Sehr kalt' }
  return { tone: 'ok', text: 'Angenehm' }
}

export function humidityStatus(pct: number | null): Assessment {
  if (pct === null) return { tone: 'danger', text: 'Sensor liefert keinen Wert' }
  if (pct < 30) return { tone: 'neutral', text: 'Trockene Luft' }
  if (pct > 80) return { tone: 'neutral', text: 'Feuchte Luft' }
  return { tone: 'ok', text: 'Normal' }
}

export function lightStatus(pct: number | null): Assessment {
  if (pct === null) return { tone: 'danger', text: 'Sensor liefert keinen Wert' }
  if (pct < 10) return { tone: 'neutral', text: 'Dunkel' }
  if (pct < 40) return { tone: 'neutral', text: 'Dämmrig' }
  return { tone: 'ok', text: 'Hell' }
}

export function signalLabel(rssi: number): { text: string; bars: 0 | 1 | 2 | 3 } {
  if (rssi >= -60) return { text: 'Sehr gut', bars: 3 }
  if (rssi >= -70) return { text: 'Gut', bars: 2 }
  if (rssi >= -80) return { text: 'Schwach', bars: 1 }
  return { text: 'Sehr schwach', bars: 0 }
}

export function secondsSince(iso: string, now: number): number {
  return Math.max(0, Math.round((now - new Date(iso).getTime()) / 1000))
}

/** "gerade eben", "vor 12 s", "vor 3 min", "vor 2 h" */
export function relativeTime(seconds: number): string {
  if (seconds < 5) return 'gerade eben'
  if (seconds < 60) return `vor ${seconds} s`
  if (seconds < 3600) return `vor ${Math.floor(seconds / 60)} min`
  if (seconds < 86400) return `vor ${Math.floor(seconds / 3600)} h`
  return `vor ${Math.floor(seconds / 86400)} Tagen`
}

export function fmtDuration(s: number): string {
  if (s < 60) return `${s} s`
  if (s < 3600) return `${Math.floor(s / 60)} min`
  if (s < 86400) return `${Math.floor(s / 3600)} h ${Math.floor((s % 3600) / 60)} min`
  return `${Math.floor(s / 86400)} Tage ${Math.floor((s % 86400) / 3600)} h`
}

export interface Overall {
  tone: Tone
  title: string
  /** Einzelne Punkte, die Aufmerksamkeit brauchen (leer = alles gut) */
  issues: Assessment[]
}

/** Gesamtzustand fuer die grosse Statuskarte oben */
export function overallStatus(r: LatestReading | null, offline: boolean): Overall {
  if (!r) return { tone: 'neutral', title: 'Warte auf die ersten Messwerte', issues: [] }
  if (offline)
    return {
      tone: 'danger',
      title: 'Keine Verbindung zum Pflanzkübel',
      issues: [{ tone: 'danger', text: 'Das Gerät hat sich länger nicht gemeldet. Ist es eingeschaltet und im WLAN?' }],
    }
  const issues: Assessment[] = []
  const soil = soilStatus(r.soil_moisture_pct)
  if (r.soil_moisture_pct !== null && soil.tone !== 'ok') issues.push({ tone: soil.tone, text: `Erde: ${soil.text}` })
  const tank = tankStatus(r.water_level_pct)
  if (tank.tone !== 'ok') issues.push({ tone: tank.tone, text: `Wassertank: ${tank.text}` })
  const temp = tempStatus(r.air_temp_c)
  if (r.air_temp_c !== null && temp.tone !== 'ok') issues.push({ tone: temp.tone, text: `Temperatur: ${temp.text}` })
  for (const e of r.errors) {
    if (e === 'tank_empty') continue // steht schon beim Tank
    issues.push({ tone: 'danger', text: describeError(e) })
  }
  if (issues.length === 0) return { tone: 'ok', title: 'Deiner Pflanze geht es gut', issues }
  const worst: Tone = issues.some((i) => i.tone === 'danger') ? 'danger' : 'warn'
  return {
    tone: worst,
    title: worst === 'danger' ? 'Deine Pflanze braucht Hilfe' : 'Bitte kurz nachsehen',
    issues,
  }
}
