import { useEffect, useState } from 'react'
import type { LatestReading } from './types'
import StatTile from './components/StatTile'
import { describeError } from './errorMessages'
import HistoryCharts from './HistoryCharts'
import DemoPanel from './DemoPanel'
import { describeDemo } from './demoText'
import { fmtCountdown, useDemo } from './useDemo'

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
  const { demo, remaining, setDemo } = useDemo(DEVICE_ID)
  const [showControls, setShowControls] = useState(() => {
    try {
      return localStorage.getItem('sg.demoControls') !== 'hidden'
    } catch {
      return true
    }
  })
  const toggleControls = () => {
    const next = !showControls
    setShowControls(next)
    try {
      localStorage.setItem('sg.demoControls', next ? 'shown' : 'hidden')
    } catch {
      /* egal */
    }
  }
  // Kacheln mit DEMO markieren: vom Geraet gemeldete Overrides + Felder, deren Sensorausfall die Demo erzwingt
  const demoFields = new Set(reading?.demo_overrides ?? [])
  for (const e of demo?.force_errors ?? []) {
    if (!reading?.errors.includes(e)) continue
    if (e.startsWith('dht')) {
      demoFields.add('air_temp_c')
      demoFields.add('air_humidity_pct')
    } else if (e.startsWith('soil')) demoFields.add('soil_moisture_pct')
    else if (e.startsWith('light')) demoFields.add('light_pct')
  }

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
      {demo && (
        <div
          role="status"
          aria-live="polite"
          className="sticky top-0 z-20 -mx-6 -mt-6 mb-6 bg-violet-700 px-6 py-2 text-sm text-white shadow dark:bg-violet-800"
        >
          <strong>DEMO-MODUS</strong> – noch {fmtCountdown(remaining)} · simuliert: {describeDemo(demo)}
        </div>
      )}

      <header className="mb-6 flex flex-wrap items-center justify-between gap-3">
        <div>
          <h1 className="text-xl font-bold text-slate-900 dark:text-slate-50">
            Smart Garden – {DEVICE_ID}
          </h1>
          <p className="text-sm text-slate-500 dark:text-slate-400">
            {reading ? `Letzter Kontakt: ${new Date(reading.received_at).toLocaleTimeString('de-DE')}` : 'Warte auf erste Daten…'}
          </p>
        </div>
        <button
          type="button"
          onClick={toggleControls}
          aria-expanded={showControls}
          aria-controls="demo-steuerung"
          className="rounded-lg border border-violet-300 px-3 py-1.5 text-sm font-medium text-violet-800 hover:bg-violet-50 focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-blue-600 dark:border-violet-700 dark:text-violet-200 dark:hover:bg-violet-950"
        >
          {showControls ? 'Demo-Steuerung ausblenden' : 'Demo-Steuerung'}
        </button>
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
          demo={demoFields.has('soil_moisture_pct')}
        />
        <StatTile
          label="Licht"
          value={fmt(reading?.light_pct ?? null, '%')}
          hint={reading?.light_raw != null ? `Rohwert: ${reading.light_raw}` : undefined}
          demo={demoFields.has('light_pct')}
        />
        <StatTile label="Lufttemperatur" value={fmt(reading?.air_temp_c ?? null, '°C')} demo={demoFields.has('air_temp_c')} />
        <StatTile label="Luftfeuchte" value={fmt(reading?.air_humidity_pct ?? null, '%')} demo={demoFields.has('air_humidity_pct')} />

        <StatTile
          label="Wassertank"
          value={fmt(reading?.water_level_pct ?? null, '%')}
          hint={reading ? `~${Math.round(reading.tank_remaining_ml)} ml (Schätzung)` : undefined}
          tone={reading ? (reading.water_level_pct <= 5 ? 'critical' : reading.water_level_pct < 20 ? 'warn' : 'ok') : 'neutral'}
          demo={demoFields.has('water_level_pct')}
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
          demo={!!demo && !!reading && reading.errors.some((e) => demo.force_errors.includes(e))}
        />
      </section>

      {showControls && (
        <div id="demo-steuerung" className="mt-8">
          <DemoPanel deviceId={DEVICE_ID} demo={demo} remaining={remaining} onChange={setDemo} reading={reading} />
        </div>
      )}

      <HistoryCharts deviceId={DEVICE_ID} />
    </div>
  )
}
