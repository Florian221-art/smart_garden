import { useEffect, useState } from 'react'
import type { LatestReading } from './types'
import StatTile from './components/StatTile'
import { describeError } from './errorMessages'
import HistoryCharts from './HistoryCharts'

const DEVICE_ID = 'esp32-kuebel-01' // Phase 1: fest; Geräteauswahl folgt in Phase 2
const POLL_INTERVAL_MS = 5000 // laut plan-webserver.md Abschnitt 6

function fmt(value: number | null | undefined, unit: string, digits = 1): string {
  if (value === null || value === undefined) return 'n/a'
  return `${value.toFixed(digits)} ${unit}`
}

function moistureTone(pct: number | null): 'ok' | 'warn' | 'critical' | 'neutral' {
  if (pct === null) return 'neutral'
  if (pct < 20) return 'critical'
  if (pct < 35) return 'warn'
  return 'ok'
}

export default function Dashboard() {
  const [reading, setReading] = useState<LatestReading | null>(null)
  const [error, setError] = useState<string | null>(null)

  useEffect(() => {
    let cancelled = false

    async function poll() {
      try {
        const res = await fetch(`/api/v1/devices/${DEVICE_ID}/latest`)
        if (!res.ok) {
          if (res.status === 404) {
            if (!cancelled) setError('Noch keine Messwerte für dieses Gerät.')
          } else {
            if (!cancelled) setError(`Server-Fehler (${res.status})`)
          }
          return
        }
        const data: LatestReading = await res.json()
        if (!cancelled) {
          setReading(data)
          setError(null)
        }
      } catch {
        if (!cancelled) setError('Server nicht erreichbar.')
      }
    }

    poll()
    const id = setInterval(poll, POLL_INTERVAL_MS)
    return () => {
      cancelled = true
      clearInterval(id)
    }
  }, [])

  return (
    <div className="min-h-screen bg-slate-50 p-6 dark:bg-slate-950">
      <header className="mb-6 flex items-center justify-between">
        <div>
          <h1 className="text-xl font-bold text-slate-900 dark:text-slate-50">
            Smart Garden – {DEVICE_ID}
          </h1>
          <p className="text-sm text-slate-500 dark:text-slate-400">
            {reading ? `Letzter Kontakt: ${new Date(reading.received_at).toLocaleTimeString('de-DE')}` : 'Warte auf erste Daten…'}
          </p>
        </div>
      </header>

      {error && (
        <div
          role="alert"
          aria-live="polite"
          className="mb-6 rounded-lg border border-amber-500/50 bg-amber-50 p-3 text-sm text-amber-800 dark:bg-amber-950/40 dark:text-amber-200"
        >
          {error}
        </div>
      )}

      <section className="grid grid-cols-1 gap-4 sm:grid-cols-2 lg:grid-cols-4">
        <StatTile
          label="Bodenfeuchte"
          value={fmt(reading?.soil_moisture_pct ?? null, '%')}
          hint={reading?.soil_moisture_raw != null ? `Rohwert: ${reading.soil_moisture_raw}` : undefined}
          tone={moistureTone(reading?.soil_moisture_pct ?? null)}
        />
        <StatTile
          label="Licht"
          value={fmt(reading?.light_pct ?? null, '%')}
          hint={reading?.light_raw != null ? `Rohwert: ${reading.light_raw}` : undefined}
        />
        <StatTile label="Lufttemperatur" value={fmt(reading?.air_temp_c ?? null, '°C')} />
        <StatTile label="Luftfeuchte" value={fmt(reading?.air_humidity_pct ?? null, '%')} />

        <StatTile
          label="Wassertank"
          value={fmt(reading?.water_level_pct ?? null, '%')}
          hint={reading ? `~${Math.round(reading.tank_remaining_ml)} ml (Schätzung)` : undefined}
          tone={reading && reading.water_level_pct < 20 ? 'warn' : 'ok'}
        />
        <StatTile
          label="Pumpe"
          value={reading ? (reading.pump_running ? 'läuft' : 'aus') : 'n/a'}
          hint={reading ? `Laufzeit seit letztem POST: ${reading.pump_on_s_since_last.toFixed(1)} s` : undefined}
        />
        <StatTile
          label="Gerät"
          value={reading ? `${reading.rssi_dbm} dBm` : 'n/a'}
          hint={reading ? `Uptime ${reading.uptime_s}s · FW ${reading.fw_version}` : undefined}
        />
        <StatTile
          label="Fehler"
          value={
            reading && reading.errors.length > 0
              ? reading.errors.map(describeError).join('; ')
              : 'keine'
          }
          hint={
            reading && reading.errors.length > 0 ? `Code(s): ${reading.errors.join(', ')}` : undefined
          }
          tone={reading && reading.errors.length > 0 ? 'critical' : 'ok'}
        />
      </section>

      {reading && reading.demo_overrides.length > 0 && (
        <div className="mt-6 rounded-lg border border-purple-500/50 bg-purple-50 p-3 text-sm text-purple-800 dark:bg-purple-950/40 dark:text-purple-200">
          DEMO-MODUS aktiv – überschrieben: {reading.demo_overrides.join(', ')}
        </div>
      )}

      <HistoryCharts deviceId={DEVICE_ID} />
    </div>
  )
}
