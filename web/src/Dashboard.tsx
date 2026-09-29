import { useEffect, useState } from 'react'
import { useTranslation } from 'react-i18next'
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
import type { DemoState, DeviceConfig, LatestReading } from './types'
import StatTile from './components/StatTile'
import StatusHero from './components/StatusHero'
import DeviceCard from './components/DeviceCard'
import WaterNowCard from './components/WaterNowCard'
import { MAX_PUMP_S } from './useWatering'
import { Button, Segmented } from './components/ui'
import HistoryCharts from './HistoryCharts'
import DemoPanel from './DemoPanel'
import { describeDemo } from './demoText'
import { fmtCountdown, useDemo } from './useDemo'
import { sendJson } from './api'
import { useTheme, type ThemePref } from './theme'
import { fmtNumber } from './format'
import { SUPPORTED_LANGUAGES, LANG_STORAGE_KEY, type Lang } from './i18n'
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
const CONFIG_POLL_MS = 60000

const LANG_LABEL: Record<Lang, string> = { de: 'DE', en: 'EN', nl: 'NL' }
// Eigenname der Sprache (nicht uebersetzt - jede Sprache nennt sich selbst so)
const LANG_NATIVE_NAME: Record<Lang, string> = { de: 'Deutsch', en: 'English', nl: 'Nederlands' }

export default function Dashboard() {
  const { t, i18n } = useTranslation()
  const [reading, setReading] = useState<LatestReading | null>(null)
  const [error, setError] = useState<{ key: string; params?: Record<string, unknown> } | null>(null)
  const [now, setNow] = useState(() => Date.now())
  const [config, setConfig] = useState<DeviceConfig | null>(null)
  const { demo, remaining, setDemo } = useDemo(DEVICE_ID)
  const { pref, setPref } = useTheme()
  const [showControls, setShowControls] = useState(() => {
    try {
      return localStorage.getItem('sg.demoControls') === 'shown'
    } catch {
      return false
    }
  })

  const THEME_OPTIONS: { value: ThemePref; label: React.ReactNode; title: string }[] = [
    { value: 'light', label: <Sun aria-hidden="true" className="size-4" />, title: t('header.theme.light') },
    { value: 'system', label: <Monitor aria-hidden="true" className="size-4" />, title: t('header.theme.system') },
    { value: 'dark', label: <Moon aria-hidden="true" className="size-4" />, title: t('header.theme.dark') },
  ]
  const LANG_OPTIONS = SUPPORTED_LANGUAGES.map((l) => ({ value: l, label: LANG_LABEL[l], title: LANG_NATIVE_NAME[l] }))

  const changeLanguage = (lang: Lang) => {
    void i18n.changeLanguage(lang)
    try {
      localStorage.setItem(LANG_STORAGE_KEY, lang)
    } catch {
      /* egal */
    }
  }

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

  // Geraete-Config (Pumpenstoss, Durchfluss, Giessschwelle) fuer "Jetzt giessen"
  useEffect(() => {
    let cancelled = false
    const load = async () => {
      try {
        const res = await fetch(`/api/v1/devices/${DEVICE_ID}/config`)
        if (res.ok && !cancelled) setConfig(await res.json())
      } catch {
        /* naechster Versuch */
      }
    }
    void load()
    const id = setInterval(load, CONFIG_POLL_MS)
    return () => {
      cancelled = true
      clearInterval(id)
    }
  }, [])

  useEffect(() => {
    let cancelled = false

    async function poll() {
      try {
        const res = await fetch(`/api/v1/devices/${DEVICE_ID}/latest`)
        if (!res.ok) {
          if (!cancelled)
            setError(
              res.status === 404
                ? { key: 'connection.notFound' }
                : { key: 'connection.serverError', params: { status: res.status } },
            )
          return
        }
        const data: LatestReading = await res.json()
        if (!cancelled) {
          setReading(data)
          setError(null)
        }
      } catch {
        if (!cancelled) setError({ key: 'connection.networkError' })
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
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [])

  const since = reading ? secondsSince(reading.received_at, now) : null
  const offline = since !== null && since > OFFLINE_AFTER_S
  const overall = overallStatus(t, reading, offline)

  const soil = reading ? soilStatus(t, reading.soil_moisture_pct) : undefined
  const tank = reading ? tankStatus(t, reading.water_level_pct) : undefined

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
              <h1 className="truncate text-base font-semibold leading-tight text-fg">{t('app.name')}</h1>
              <p className="truncate text-sm text-muted">{t('app.pot')}</p>
            </div>
          </div>
          <div className="flex shrink-0 flex-wrap items-center justify-end gap-1 sm:gap-2">
            <Button
              variant={showControls ? 'secondary' : 'ghost'}
              icon={FlaskConical}
              onClick={toggleControls}
              aria-expanded={showControls}
              aria-controls="demo-steuerung"
              aria-label={t('header.demoAriaLabel')}
              className="px-3"
            >
              <span className="hidden sm:inline">{t('header.demoButton')}</span>
            </Button>
            <Segmented
              label={t('header.languageLabel')}
              size="sm"
              options={LANG_OPTIONS}
              value={(SUPPORTED_LANGUAGES.find((l) => l === i18n.language) ?? 'de') as Lang}
              onChange={changeLanguage}
            />
            <Segmented label={t('header.themeLabel')} size="sm" options={THEME_OPTIONS} value={pref} onChange={setPref} />
          </div>
        </div>

        {demo && (
          <div role="status" aria-live="polite" className="bg-inverse text-inverse-fg">
            <div className="mx-auto flex max-w-6xl items-center gap-3 px-[max(1rem,env(safe-area-inset-left))] py-2 text-sm sm:px-6">
              <FlaskConical aria-hidden="true" className="size-4 shrink-0" />
              <p className="min-w-0 flex-1">
                <strong className="font-semibold">{t('demoBar.running')}</strong>
                <span className="tabular-nums"> · {t('demoBar.remaining', { time: fmtCountdown(remaining) })}</span>
                <span className="hidden sm:inline"> · {t('demoBar.simulated', { text: describeDemo(t, i18n.language, demo) })}</span>
              </p>
              <button
                type="button"
                onClick={stopDemo}
                className="inline-flex min-h-9 shrink-0 items-center gap-1.5 rounded-lg px-2.5 font-medium hover:bg-white/10 dark:hover:bg-black/10"
              >
                <X aria-hidden="true" className="size-4" />
                {t('demoBar.stop')}
              </button>
            </div>
          </div>
        )}
      </header>

      <main className="mx-auto flex max-w-6xl flex-col gap-8 px-[max(1rem,env(safe-area-inset-left))] pb-[max(2rem,env(safe-area-inset-bottom))] pt-6 sm:px-6">
        <StatusHero
          overall={overall}
          since={since}
          offline={offline}
          pumpRunning={!!reading?.pump_running}
          error={error ? t(error.key, error.params) : null}
        />

        <WaterNowCard deviceId={DEVICE_ID} reading={reading} offline={offline} config={config} now={now} />

        {showControls && (
          <div id="demo-steuerung">
            <DemoPanel
              deviceId={DEVICE_ID}
              demo={demo}
              remaining={remaining}
              onChange={setDemo}
              reading={reading}
              pumpS={Math.min(config?.max_pump_s_per_run ?? MAX_PUMP_S, MAX_PUMP_S)}
            />
          </div>
        )}

        <section aria-labelledby="werte-titel">
          <h2 id="werte-titel" className="sr-only">
            {t('values.sectionTitle')}
          </h2>
          <div className="grid grid-cols-2 gap-3 sm:gap-4 lg:grid-cols-3">
            <StatTile
              icon={Droplets}
              label={t('tiles.soilMoisture')}
              value={reading?.soil_moisture_pct != null ? fmtNumber(i18n.language, reading.soil_moisture_pct, 0) : null}
              unit="%"
              meter={reading ? reading.soil_moisture_pct : undefined}
              status={soil}
              demo={demoFields.has('soil_moisture_pct')}
            />
            <StatTile
              icon={GlassWater}
              label={t('tiles.waterTank')}
              value={reading ? fmtNumber(i18n.language, reading.water_level_pct, 0) : null}
              unit="%"
              meter={reading ? reading.water_level_pct : undefined}
              status={tank}
              hint={reading ? t('tiles.tankHint', { ml: Math.round(reading.tank_remaining_ml) }) : undefined}
              demo={demoFields.has('water_level_pct')}
            />
            <StatTile
              icon={Thermometer}
              label={t('tiles.temperature')}
              value={reading?.air_temp_c != null ? fmtNumber(i18n.language, reading.air_temp_c, 1) : null}
              unit="°C"
              status={reading ? tempStatus(t, reading.air_temp_c) : undefined}
              demo={demoFields.has('air_temp_c')}
            />
            <StatTile
              icon={CloudDrizzle}
              label={t('tiles.humidity')}
              value={reading?.air_humidity_pct != null ? fmtNumber(i18n.language, reading.air_humidity_pct, 0) : null}
              unit="%"
              status={reading ? humidityStatus(t, reading.air_humidity_pct) : undefined}
              demo={demoFields.has('air_humidity_pct')}
            />
            <StatTile
              icon={Sun}
              label={t('tiles.light')}
              value={reading?.light_pct != null ? fmtNumber(i18n.language, reading.light_pct, 0) : null}
              unit="%"
              status={reading ? lightStatus(t, reading.light_pct) : undefined}
              demo={demoFields.has('light_pct')}
            />
            <StatTile
              icon={ShowerHead}
              label={t('tiles.watering')}
              value={reading ? (reading.pump_running ? t('tiles.wateringRunningValue') : t('tiles.wateringOffValue')) : null}
              status={
                reading
                  ? reading.pump_running
                    ? { tone: 'ok', text: t('tiles.wateringStatusOn') }
                    : { tone: 'neutral', text: t('tiles.wateringStatusOff') }
                  : undefined
              }
              hint={
                reading && reading.pump_on_s_since_last > 0
                  ? t('tiles.pumpRunningSince', { s: fmtNumber(i18n.language, reading.pump_on_s_since_last, 1) })
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
