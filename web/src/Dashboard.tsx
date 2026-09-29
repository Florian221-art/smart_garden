import { useEffect, useState } from 'react'
import {
  CloudDrizzle,
  Cpu,
  Droplets,
  FlaskConical,
  GlassWater,
  Monitor,
  Moon,
  ShowerHead,
  Sprout,
  Sun,
  Thermometer,
  X,
} from 'lucide-react'
import type { DemoState, LatestReading } from './types'
import StatTile from './components/StatTile'
import StatusHero from './components/StatusHero'
import DeviceCard from './components/DeviceCard'
import { Button, Segmented } from './components/ui'
import HistoryCharts from './HistoryCharts'
import DemoPanel from './DemoPanel'
import { describeDemo } from './demoText'
import { fmtCountdown, useDemo } from './useDemo'
import { sendJson } from './api'
import { useTheme, type ThemePref } from './theme'
import {
  OFFLINE_AFTER_S,
  humidityStatus,
  lightStatus,
  overallStatus,
  secondsSince,
  soilStatus,
  tankStatus,
  tempStatus,
} from './status'

const DEVICE_ID = 'esp32-kuebel-01' // Phase 1: fest; Geraeteauswahl folgt in Phase 2
const POLL_INTERVAL_MS = 5000 // laut plan-webserver.md Abschnitt 6

function num(value: number | null | undefined, digits = 0): string | null {
  if (value === null || value === undefined) return null
  return value.toFixed(digits).replace('.', ',')
}

const THEME_OPTIONS: { value: ThemePref; label: React.ReactNode; title: string }[] = [
  { value: 'light', label: <Sun aria-hidden="true" className="size-4" />, title: 'Hell' },
  { value: 'system', label: <Monitor aria-hidden="true" className="size-4" />, title: 'Wie das Gerät' },
  { value: 'dark', label: <Moon aria-hidden="true" className="size-4" />, title: 'Dunkel' },
]

export default function Dashboard() {
  const [reading, setReading] = useState<LatestReading | null>(null)
  const [error, setError] = useState<string | null>(null)
  const [now, setNow] = useState(() => Date.now())
  const { demo, remaining, setDemo } = useDemo(DEVICE_ID)
  const { pref, setPref } = useTheme()
  const [showControls, setShowControls] = useState(() => {
    try {
      return localStorage.getItem('sg.demoControls') === 'shown'
    } catch {
      return false
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

  const stopDemo = async () => {
    try {
      setDemo(await sendJson<DemoState>(`/api/v1/devices/${DEVICE_ID}/demo`, 'DELETE'))
    } catch {
      /* Fehler zeigt die Demo-Steuerung */
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
          if (!cancelled)
            setError(
              res.status === 404
                ? 'Der Pflanzkübel hat noch keine Messwerte geschickt. Sobald er eingeschaltet und im WLAN ist, erscheinen sie hier.'
                : `Der Server meldet einen Fehler (${res.status}). Bitte später erneut versuchen.`,
            )
          return
        }
        const data: LatestReading = await res.json()
        if (!cancelled) {
          setReading(data)
          setError(null)
        }
      } catch {
        if (!cancelled) setError('Der Server ist nicht erreichbar. Bist du im WLAN „SmartGarden“?')
      }
    }

    poll()
    const id = setInterval(poll, POLL_INTERVAL_MS)
    const tick = setInterval(() => setNow(Date.now()), 1000)
    return () => {
      cancelled = true
      clearInterval(id)
      clearInterval(tick)
    }
  }, [])

  const since = reading ? secondsSince(reading.received_at, now) : null
  const offline = since !== null && since > OFFLINE_AFTER_S
  const overall = overallStatus(reading, offline)

  const soil = reading ? soilStatus(reading.soil_moisture_pct) : undefined
  const tank = reading ? tankStatus(reading.water_level_pct) : undefined

  return (
    <div className="min-h-screen bg-bg text-fg">
      {/* Kopfzeile */}
      <header className="sticky top-0 z-30 border-b border-line bg-surface/90 backdrop-blur supports-[backdrop-filter]:bg-surface/80">
        <div className="mx-auto flex max-w-6xl items-center justify-between gap-2 px-[max(1rem,env(safe-area-inset-left))] pb-3 pt-[max(0.75rem,env(safe-area-inset-top))] sm:px-6">
          <div className="flex min-w-0 items-center gap-2.5 sm:gap-3">
            <span className="grid size-9 shrink-0 sm:size-10 place-items-center rounded-xl bg-accent text-accent-fg">
              <Sprout aria-hidden="true" className="size-5" />
            </span>
            <div className="min-w-0">
              <h1 className="truncate text-base font-semibold leading-tight text-fg">Smart Garden</h1>
              <p className="truncate text-sm text-muted">Kübel 1</p>
            </div>
          </div>
          <div className="flex shrink-0 items-center gap-1 sm:gap-2">
            <Button
              variant={showControls ? 'secondary' : 'ghost'}
              icon={FlaskConical}
              onClick={toggleControls}
              aria-expanded={showControls}
              aria-controls="demo-steuerung"
              aria-label="Demo-Steuerung"
              className="px-3"
            >
              <span className="hidden sm:inline">Demo</span>
            </Button>
            <Segmented label="Farbschema" size="sm" options={THEME_OPTIONS} value={pref} onChange={setPref} />
          </div>
        </div>

        {demo && (
          <div role="status" aria-live="polite" className="bg-inverse text-inverse-fg">
            <div className="mx-auto flex max-w-6xl items-center gap-3 px-[max(1rem,env(safe-area-inset-left))] py-2 text-sm sm:px-6">
              <FlaskConical aria-hidden="true" className="size-4 shrink-0" />
              <p className="min-w-0 flex-1">
                <strong className="font-semibold">Demo läuft</strong>
                <span className="tabular-nums"> · noch {fmtCountdown(remaining)}</span>
                <span className="hidden sm:inline"> · simuliert: {describeDemo(demo)}</span>
              </p>
              <button
                type="button"
                onClick={stopDemo}
                className="inline-flex min-h-9 shrink-0 items-center gap-1.5 rounded-lg px-2.5 font-medium hover:bg-white/10 dark:hover:bg-black/10"
              >
                <X aria-hidden="true" className="size-4" />
                Beenden
              </button>
            </div>
          </div>
        )}
      </header>

      <main className="mx-auto flex max-w-6xl flex-col gap-8 px-[max(1rem,env(safe-area-inset-left))] pb-[max(2rem,env(safe-area-inset-bottom))] pt-6 sm:px-6">
        <StatusHero overall={overall} since={since} offline={offline} pumpRunning={!!reading?.pump_running} error={error} />

        {showControls && (
          <div id="demo-steuerung">
            <DemoPanel deviceId={DEVICE_ID} demo={demo} remaining={remaining} onChange={setDemo} reading={reading} />
          </div>
        )}

        <section aria-labelledby="werte-titel">
          <h2 id="werte-titel" className="sr-only">
            Aktuelle Werte
          </h2>
          <div className="grid grid-cols-2 gap-3 sm:gap-4 lg:grid-cols-3">
            <StatTile
              icon={Droplets}
              label="Bodenfeuchte"
              value={num(reading?.soil_moisture_pct)}
              unit="%"
              meter={reading ? reading.soil_moisture_pct : undefined}
              status={soil}
              demo={demoFields.has('soil_moisture_pct')}
            />
            <StatTile
              icon={GlassWater}
              label="Wassertank"
              value={num(reading?.water_level_pct)}
              unit="%"
              meter={reading ? reading.water_level_pct : undefined}
              status={tank}
              hint={reading ? `ca. ${Math.round(reading.tank_remaining_ml)} ml (geschätzt)` : undefined}
              demo={demoFields.has('water_level_pct')}
            />
            <StatTile
              icon={Thermometer}
              label="Temperatur"
              value={num(reading?.air_temp_c, 1)}
              unit="°C"
              status={reading ? tempStatus(reading.air_temp_c) : undefined}
              demo={demoFields.has('air_temp_c')}
            />
            <StatTile
              icon={CloudDrizzle}
              label="Luftfeuchte"
              value={num(reading?.air_humidity_pct)}
              unit="%"
              status={reading ? humidityStatus(reading.air_humidity_pct) : undefined}
              demo={demoFields.has('air_humidity_pct')}
            />
            <StatTile
              icon={Sun}
              label="Licht"
              value={num(reading?.light_pct)}
              unit="%"
              status={reading ? lightStatus(reading.light_pct) : undefined}
              demo={demoFields.has('light_pct')}
            />
            <StatTile
              icon={ShowerHead}
              label="Bewässerung"
              value={reading ? (reading.pump_running ? 'Läuft' : 'Aus') : null}
              status={
                reading
                  ? reading.pump_running
                    ? { tone: 'ok', text: 'Gießt gerade' }
                    : { tone: 'neutral', text: 'Bereit' }
                  : undefined
              }
              hint={
                reading && reading.pump_on_s_since_last > 0
                  ? `Seit letzter Meldung ${reading.pump_on_s_since_last.toFixed(1).replace('.', ',')} s gegossen`
                  : undefined
              }
            />
          </div>
        </section>

        <HistoryCharts deviceId={DEVICE_ID} />

        {reading && <DeviceCard icon={Cpu} reading={reading} since={since ?? 0} offline={offline} />}
      </main>
    </div>
  )
}
