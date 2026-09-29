import { useCallback, useEffect, useState } from 'react'
import { useTranslation } from 'react-i18next'
import { sendJson } from './api'
import { describeError } from './errorMessages'
import type { DemoState, LatestReading, OverrideField, PendingCommands } from './types'
import { describeDemo, fieldLabel, FIELDS } from './demoText'
import { fmtCountdown } from './useDemo'
import type { LucideIcon } from 'lucide-react'
import {
  BellRing,
  CircleCheck,
  DropletOff,
  FlaskConical,
  Flame,
  GlassWater,
  Hourglass,
  Lightbulb,
  Moon,
  RefreshCw,
  ShowerHead,
  SlidersHorizontal,
  Unplug,
  X,
} from 'lucide-react'
import { Button, Card, Segmented } from './components/ui'
import { decimalSeparator } from './format'

interface Scenario {
  id: string
  titleKey: string
  icon: LucideIcon
  effectKey: string
  overrides?: Partial<Record<OverrideField, number>>
  force_errors?: string[]
  hintKey?: string
}

// Szenarien aus plan-webserver.md Abschnitt 5 (Texte kommen aus i18n: demo.scenario.*)
const SCENARIOS: Scenario[] = [
  {
    id: 'trockene_erde',
    icon: DropletOff,
    titleKey: 'demo.scenario.dry.title',
    effectKey: 'demo.scenario.dry.effect',
    overrides: { soil_moisture_pct: 12 },
  },
  {
    id: 'hitzewelle',
    icon: Flame,
    titleKey: 'demo.scenario.heat.title',
    effectKey: 'demo.scenario.heat.effect',
    overrides: { air_temp_c: 38, air_humidity_pct: 25 },
  },
  {
    id: 'tank_leer',
    icon: GlassWater,
    titleKey: 'demo.scenario.tankLow.title',
    effectKey: 'demo.scenario.tankLow.effect',
    overrides: { water_level_pct: 4 },
    hintKey: 'demo.scenario.tankLow.hint',
  },
  {
    id: 'sensorausfall',
    icon: Unplug,
    titleKey: 'demo.scenario.sensorFail.title',
    effectKey: 'demo.scenario.sensorFail.effect',
    force_errors: ['dht_read_failed'],
  },
  {
    id: 'nacht',
    icon: Moon,
    titleKey: 'demo.scenario.night.title',
    effectKey: 'demo.scenario.night.effect',
    overrides: { light_pct: 2 },
  },
]

const ERRORS = ['dht_read_failed', 'soil_out_of_range', 'light_read_failed']

const DURATIONS = [60, 120, 300, 600]

interface Props {
  deviceId: string
  demo: DemoState | null
  remaining: number
  onChange: (d: DemoState) => void
  reading: LatestReading | null
}

export default function DemoPanel({ deviceId, demo, remaining, onChange, reading }: Props) {
  const { t, i18n } = useTranslation()
  const base = `/api/v1/devices/${deviceId}`
  const [duration, setDuration] = useState(120)
  const [busy, setBusy] = useState(false)
  const [message, setMessage] = useState<{ kind: 'ok' | 'error'; text: string } | null>(null)
  const [custom, setCustom] = useState<Record<OverrideField, { on: boolean; value: number }>>(
    () => Object.fromEntries(FIELDS.map((f) => [f.key, { on: false, value: f.start }])) as Record<OverrideField, { on: boolean; value: number }>,
  )
  const [customErrors, setCustomErrors] = useState<string[]>([])
  const [pending, setPending] = useState<PendingCommands | null>(null)

  const loadPending = useCallback(async () => {
    try {
      const res = await fetch(`${base}/commands`)
      if (res.ok) setPending(await res.json())
    } catch {
      /* ignorieren */
    }
  }, [base])

  useEffect(() => {
    loadPending()
    const id = setInterval(loadPending, 3000)
    return () => clearInterval(id)
  }, [loadPending])

  // Erfolgsmeldung nach ein paar Sekunden ausblenden (Fehler bleiben stehen)
  useEffect(() => {
    if (message?.kind !== 'ok') return
    const id = setTimeout(() => setMessage(null), 6000)
    return () => clearTimeout(id)
  }, [message])

  async function run(action: () => Promise<void>, okText: string) {
    setBusy(true)
    setMessage(null)
    try {
      await action()
      setMessage({ kind: 'ok', text: okText })
    } catch (e) {
      setMessage({ kind: 'error', text: e instanceof Error ? e.message : String(e) })
    } finally {
      setBusy(false)
    }
  }

  const startDemo = (body: { overrides?: Partial<Record<OverrideField, number>>; force_errors?: string[]; scenario?: string }, label: string) =>
    run(async () => {
      const d = await sendJson<DemoState>(`${base}/demo`, 'PUT', { ...body, duration_s: duration })
      onChange(d)
    }, t('demo.startMessage', { label }))

  const stopDemo = () =>
    run(async () => {
      const d = await sendJson<DemoState>(`${base}/demo`, 'DELETE')
      onChange(d)
    }, t('demo.stopMessage'))

  const command = (body: Record<string, unknown>, label: string) =>
    run(async () => {
      setPending(await sendJson<PendingCommands>(`${base}/commands`, 'POST', body))
    }, t('demo.commandMessage', { label }))

  const sendCustom = () => {
    const overrides = Object.fromEntries(
      FIELDS.filter((f) => custom[f.key].on).map((f) => [f.key, custom[f.key].value]),
    ) as Partial<Record<OverrideField, number>>
    return startDemo({ overrides, force_errors: customErrors, scenario: 'eigene_werte' }, t('demo.scenario.custom.title'))
  }
  const customEmpty = !FIELDS.some((f) => custom[f.key].on) && customErrors.length === 0

  // Hat das Geraet die Demo schon uebernommen? (Meldung nach dem Start mit Demo-Feldern oder erzwungenem Fehler)
  const confirmed =
    !!demo &&
    !!reading &&
    !!demo.started_at &&
    new Date(reading.received_at) > new Date(demo.started_at) &&
    (reading.demo_overrides.length > 0 || demo.force_errors.some((e) => reading.errors.includes(e)))

  const activeScenario = SCENARIOS.find((s) => s.id === demo?.scenario)
  const sep = decimalSeparator(i18n.language)
  const pendingList = pending
    ? [
        pending.pump_run_s > 0 && t('demo.pending.watering', { s: pending.pump_run_s }),
        pending.tank_refilled && t('demo.pending.refilled'),
        pending.identify && t('demo.pending.identify'),
        pending.buzzer && t('demo.pending.buzz'),
      ].filter((v): v is string => typeof v === 'string')
    : []

  return (
    <Card as="section" aria-labelledby="demo-titel" className="p-4 sm:p-6">
      <div className="mb-4 flex flex-wrap items-start justify-between gap-4">
        <div className="flex min-w-0 items-start gap-3">
          <span className="grid size-10 shrink-0 place-items-center rounded-xl bg-surface-2 text-fg-2">
            <FlaskConical aria-hidden="true" className="size-5" />
          </span>
          <div className="min-w-0">
            <h2 id="demo-titel" className="text-base font-semibold text-fg">
              {t('demo.title')}
            </h2>
            <p className="mt-0.5 max-w-2xl text-sm text-fg-2">{t('demo.description')}</p>
          </div>
        </div>
        <div className="flex items-center gap-2 text-sm text-fg-2">
          <span id="dauer-label">{t('demo.durationLabel')}</span>
          <Segmented
            label={t('demo.durationLabel')}
            options={DURATIONS.map((s) => ({ value: s, label: t(`demo.duration.${s}`) }))}
            value={duration}
            onChange={setDuration}
          />
        </div>
      </div>

      {/* Status */}
      <div
        className={`mb-5 flex flex-wrap items-center justify-between gap-3 rounded-xl p-3 text-sm sm:p-4 ${
          demo ? 'bg-accent-soft text-accent-ink' : 'bg-surface-2 text-fg-2'
        }`}
        role="status"
        aria-live="polite"
      >
        {demo ? (
          <div className="min-w-0">
            <div className="font-semibold tabular-nums">
              {activeScenario
                ? t('demo.statusRunningNamed', { scenario: t(activeScenario.titleKey), time: fmtCountdown(remaining) })
                : t('demo.statusRunning', { time: fmtCountdown(remaining) })}
            </div>
            <div className="opacity-80">{describeDemo(t, i18n.language, demo)}</div>
            <div className="mt-1 flex items-center gap-1.5 text-xs opacity-80">
              {confirmed ? (
                <>
                  <CircleCheck aria-hidden="true" className="size-3.5" /> {t('demo.confirmed')}
                </>
              ) : (
                <>
                  <Hourglass aria-hidden="true" className="size-3.5" /> {t('demo.waiting')}
                </>
              )}
            </div>
          </div>
        ) : (
          <div className="flex items-center gap-2">
            <CircleCheck aria-hidden="true" className="size-4 text-accent" />
            {t('demo.inactive')}
          </div>
        )}
        {demo && (
          <button
            type="button"
            onClick={stopDemo}
            disabled={busy}
            className="inline-flex min-h-11 items-center gap-1.5 rounded-xl border border-current/40 bg-surface/40 px-3.5 font-medium hover:bg-surface disabled:opacity-50"
          >
            <X aria-hidden="true" className="size-4" />
            {t('demo.stop')}
          </button>
        )}
      </div>

      {/* Szenarien */}
      <h3 className="mb-2 text-sm font-semibold text-fg">{t('demo.scenariosTitle')}</h3>
      <div className="grid grid-cols-1 gap-2 min-[420px]:grid-cols-2 lg:grid-cols-5">
        {SCENARIOS.map((s) => {
          const isActive = demo?.scenario === s.id
          const Icon = s.icon
          const title = t(s.titleKey)
          return (
            <button
              key={s.id}
              type="button"
              disabled={busy}
              aria-pressed={isActive}
              onClick={() => startDemo({ overrides: s.overrides ?? {}, force_errors: s.force_errors ?? [], scenario: s.id }, title)}
              className={`flex items-start gap-3 rounded-xl border p-3 text-left transition-colors disabled:opacity-50 lg:flex-col lg:gap-2 ${
                isActive ? 'border-accent bg-accent-soft' : 'border-line bg-surface hover:border-control hover:bg-surface-2'
              }`}
            >
              <span
                className={`grid size-9 shrink-0 place-items-center rounded-lg ${
                  isActive ? 'bg-accent text-accent-fg' : 'bg-surface-2 text-fg-2'
                }`}
              >
                <Icon aria-hidden="true" className="size-[18px]" />
              </span>
              <span className="min-w-0">
                <span className={`block text-sm font-semibold ${isActive ? 'text-accent-ink' : 'text-fg'}`}>
                  {title}
                  {isActive && <span className="sr-only">{t('demo.activeSuffix')}</span>}
                </span>
                <span className="mt-0.5 block text-xs text-fg-2 [overflow-wrap:anywhere]">{t(s.effectKey)}</span>
                {s.hintKey && <span className="mt-1 block text-xs text-muted [overflow-wrap:anywhere]">{t(s.hintKey)}</span>}
              </span>
            </button>
          )
        })}
      </div>

      {/* Eigene Werte */}
      <details className="group mt-5 rounded-xl border border-line">
        <summary className="flex min-h-12 cursor-pointer list-none items-center gap-2 px-4 text-sm font-semibold text-fg [&::-webkit-details-marker]:hidden">
          <SlidersHorizontal aria-hidden="true" className="size-4 text-muted" />
          {t('demo.customValuesTitle')}
        </summary>
        <div className="border-t border-line p-4">
          <div className="grid grid-cols-1 gap-x-8 gap-y-2 md:grid-cols-2">
            {FIELDS.map((f) => {
              const c = custom[f.key]
              const id = `demo-${f.key}`
              const label = fieldLabel(t, f.key)
              return (
                <div key={f.key} className="flex items-center gap-3 text-sm">
                  <label className="flex min-h-11 w-36 shrink-0 items-center gap-2 text-fg">
                    <input
                      type="checkbox"
                      checked={c.on}
                      onChange={(e) => setCustom({ ...custom, [f.key]: { ...c, on: e.target.checked } })}
                      className="size-4 accent-[var(--accent)]"
                    />
                    {label}
                  </label>
                  <input
                    id={id}
                    type="range"
                    aria-label={`${label} in ${f.unit}`}
                    min={f.min}
                    max={f.max}
                    step={f.step}
                    value={c.value}
                    disabled={!c.on}
                    onChange={(e) => setCustom({ ...custom, [f.key]: { ...c, value: Number(e.target.value) } })}
                    className="w-full accent-[var(--accent)] disabled:opacity-40"
                  />
                  <output htmlFor={id} className="w-16 shrink-0 text-right tabular-nums text-fg">
                    {String(c.value).replace('.', sep)} {f.unit}
                  </output>
                </div>
              )
            })}
          </div>
          <fieldset className="mt-4 text-sm">
            <legend className="mb-1 font-medium text-fg">{t('demo.forceErrorsLegend')}</legend>
            <div className="flex flex-wrap gap-x-6">
              {ERRORS.map((e) => (
                <label key={e} className="flex min-h-11 items-center gap-2 text-fg-2">
                  <input
                    type="checkbox"
                    className="size-4 accent-[var(--accent)]"
                    checked={customErrors.includes(e)}
                    onChange={(ev) =>
                      setCustomErrors(ev.target.checked ? [...customErrors, e] : customErrors.filter((x) => x !== e))
                    }
                  />
                  {describeError(t, e)}
                </label>
              ))}
            </div>
          </fieldset>
          <Button variant="primary" onClick={sendCustom} disabled={busy || customEmpty} className="mt-3">
            {t('demo.sendCustom')}
          </Button>
        </div>
      </details>

      {/* Geraetebefehle */}
      <h3 className="mb-2 mt-5 text-sm font-semibold text-fg">{t('demo.deviceControlTitle')}</h3>
      <div className="grid grid-cols-2 gap-2 sm:flex sm:flex-wrap">
        <Button icon={ShowerHead} disabled={busy} onClick={() => command({ pump_run_s: 5 }, t('demo.waterCommandLabel'))}>
          {t('demo.water')}
        </Button>
        <Button icon={RefreshCw} disabled={busy} onClick={() => command({ tank_refilled: true }, t('demo.refill'))}>
          {t('demo.refill')}
        </Button>
        <Button icon={Lightbulb} disabled={busy} onClick={() => command({ identify: true }, t('demo.identify'))}>
          {t('demo.identify')}
        </Button>
        <Button icon={BellRing} disabled={busy} onClick={() => command({ buzzer: true }, t('demo.buzz'))}>
          {t('demo.buzz')}
        </Button>
      </div>
      {pendingList.length > 0 && (
        <p className="mt-2 flex items-center gap-1.5 text-xs text-muted">
          <Hourglass aria-hidden="true" className="size-3.5" />
          {t('demo.pending.prefix', { list: pendingList.join(', ') })}
        </p>
      )}

      {message && (
        <p
          role={message.kind === 'error' ? 'alert' : 'status'}
          className={`mt-4 flex items-start gap-1.5 text-sm font-medium ${message.kind === 'error' ? 'text-danger' : 'text-accent'}`}
        >
          {message.kind === 'ok' && <CircleCheck aria-hidden="true" className="mt-0.5 size-4 shrink-0" />}
          {message.text}
        </p>
      )}
    </Card>
  )
}
