import { useCallback, useEffect, useState } from 'react'
import { sendJson } from './api'
import { describeError } from './errorMessages'
import type { DemoState, LatestReading, OverrideField, PendingCommands } from './types'
import { describeDemo, FIELDS } from './demoText'
import { fmtCountdown } from './useDemo'

interface Scenario {
  id: string
  title: string
  effect: string
  overrides?: Partial<Record<OverrideField, number>>
  force_errors?: string[]
  hint?: string
}

// Szenarien aus plan-webserver.md Abschnitt 5
const SCENARIOS: Scenario[] = [
  {
    id: 'trockene_erde',
    title: 'Trockene Erde',
    effect: 'Bodenfeuchte 12 % → LED-Bar rot, ESP gießt automatisch',
    overrides: { soil_moisture_pct: 12 },
  },
  {
    id: 'hitzewelle',
    title: 'Hitzewelle',
    effect: '38 °C und 25 % Luftfeuchte',
    overrides: { air_temp_c: 38, air_humidity_pct: 25 },
  },
  {
    id: 'tank_leer',
    title: 'Tank fast leer',
    effect: 'Tank 4 % → Summer, Auto-Bewässerung stoppt',
    overrides: { water_level_pct: 4 },
    hint: 'Danach „Tank aufgefüllt“ drücken – der ESP merkt sich die Tank-Schätzung.',
  },
  {
    id: 'sensorausfall',
    title: 'Sensorausfall',
    effect: 'Temperatur-/Luftfeuchtesensor liefert nichts',
    force_errors: ['dht_read_failed'],
  },
  {
    id: 'nacht',
    title: 'Nacht',
    effect: 'Licht 2 %',
    overrides: { light_pct: 2 },
  },
]

const ERRORS = ['dht_read_failed', 'soil_out_of_range', 'light_read_failed']

const DURATIONS = [
  { s: 60, label: '1 min' },
  { s: 120, label: '2 min' },
  { s: 300, label: '5 min' },
  { s: 600, label: '10 min' },
]

const btn =
  'rounded-lg border px-3 py-2 text-sm font-medium focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-blue-600 disabled:cursor-not-allowed disabled:opacity-50'
const btnNeutral = `${btn} border-slate-300 bg-white text-slate-800 hover:bg-slate-50 dark:border-slate-600 dark:bg-slate-800 dark:text-slate-100 dark:hover:bg-slate-700`

interface Props {
  deviceId: string
  demo: DemoState | null
  remaining: number
  onChange: (d: DemoState) => void
  reading: LatestReading | null
}

export default function DemoPanel({ deviceId, demo, remaining, onChange, reading }: Props) {
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
    }, `„${label}“ gestartet – das Gerät übernimmt es mit seiner nächsten Meldung.`)

  const stopDemo = () =>
    run(async () => {
      const d = await sendJson<DemoState>(`${base}/demo`, 'DELETE')
      onChange(d)
    }, 'Demo beendet – ab der nächsten Meldung wieder echte Werte.')

  const command = (body: Record<string, unknown>, label: string) =>
    run(async () => {
      setPending(await sendJson<PendingCommands>(`${base}/commands`, 'POST', body))
    }, `„${label}“ wird mit der nächsten Meldung an das Gerät geschickt.`)

  const sendCustom = () => {
    const overrides = Object.fromEntries(
      FIELDS.filter((f) => custom[f.key].on).map((f) => [f.key, custom[f.key].value]),
    ) as Partial<Record<OverrideField, number>>
    return startDemo({ overrides, force_errors: customErrors, scenario: 'eigene_werte' }, 'Eigene Werte')
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
  const pendingList = pending
    ? [
        pending.pump_run_s > 0 && `Gießen ${pending.pump_run_s} s`,
        pending.tank_refilled && 'Tank aufgefüllt',
        pending.identify && 'Gerät finden',
        pending.buzzer && 'Piepen',
      ].filter(Boolean)
    : []

  return (
    <section
      aria-labelledby="demo-titel"
      className="rounded-xl border border-violet-300 bg-white p-4 shadow-sm dark:border-violet-800 dark:bg-slate-900"
    >
      <div className="mb-3 flex flex-wrap items-center justify-between gap-3">
        <div>
          <h2 id="demo-titel" className="text-lg font-bold text-slate-900 dark:text-slate-50">
            Demo-Steuerung
          </h2>
          <p className="text-sm text-slate-600 dark:text-slate-300">
            Überschreibt Messwerte auf dem Gerät – Pumpe, LED-Bar und Summer reagieren wie echt. Die Sicherheitsgrenzen
            der Pumpe gelten weiter.
          </p>
        </div>
        <div role="group" aria-label="Dauer" className="flex items-center gap-2 text-sm text-slate-700 dark:text-slate-300">
          <span>Dauer</span>
          <div className="inline-flex rounded-lg border border-slate-300 p-0.5 dark:border-slate-700">
            {DURATIONS.map((d) => (
              <button
                key={d.s}
                type="button"
                aria-pressed={duration === d.s}
                onClick={() => setDuration(d.s)}
                className={`rounded-md px-2.5 py-1 focus-visible:outline-2 focus-visible:outline-blue-600 ${
                  duration === d.s
                    ? 'bg-slate-900 text-white dark:bg-slate-100 dark:text-slate-900'
                    : 'hover:bg-slate-100 dark:hover:bg-slate-800'
                }`}
              >
                {d.label}
              </button>
            ))}
          </div>
        </div>
      </div>

      {/* Status */}
      <div
        className={`mb-4 flex flex-wrap items-center justify-between gap-3 rounded-lg p-3 text-sm ${
          demo
            ? 'bg-violet-50 text-violet-900 dark:bg-violet-950/50 dark:text-violet-100'
            : 'bg-slate-50 text-slate-700 dark:bg-slate-800 dark:text-slate-300'
        }`}
        role="status"
        aria-live="polite"
      >
        {demo ? (
          <div>
            <div className="font-semibold">
              Aktiv{activeScenario ? `: ${activeScenario.title}` : ''} – noch {fmtCountdown(remaining)}
            </div>
            <div>{describeDemo(demo)}</div>
            <div className="mt-1 text-xs">
              {confirmed ? '✓ Gerät zeigt die Demo-Werte' : '… wartet auf die nächste Meldung des Geräts (alle ~15 s)'}
            </div>
          </div>
        ) : (
          <div>Aus – alle Werte sind echt gemessen.</div>
        )}
        <button type="button" onClick={stopDemo} disabled={busy || !demo} className={btnNeutral}>
          Demo beenden
        </button>
      </div>

      {/* Szenarien */}
      <h3 className="mb-2 text-sm font-semibold text-slate-900 dark:text-slate-50">Szenarien (ein Klick)</h3>
      <div className="grid grid-cols-1 gap-2 sm:grid-cols-2 lg:grid-cols-5">
        {SCENARIOS.map((s) => {
          const isActive = demo?.scenario === s.id
          return (
            <button
              key={s.id}
              type="button"
              disabled={busy}
              aria-pressed={isActive}
              onClick={() => startDemo({ overrides: s.overrides ?? {}, force_errors: s.force_errors ?? [], scenario: s.id }, s.title)}
              className={`rounded-lg border p-3 text-left focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-blue-600 disabled:opacity-50 ${
                isActive
                  ? 'border-violet-600 bg-violet-50 dark:border-violet-400 dark:bg-violet-950/50'
                  : 'border-slate-300 bg-white hover:border-violet-400 hover:bg-violet-50/50 dark:border-slate-600 dark:bg-slate-800 dark:hover:bg-slate-700'
              }`}
            >
              <div className="text-sm font-semibold text-slate-900 dark:text-slate-50">
                {isActive && <span aria-hidden="true">● </span>}
                {s.title}
              </div>
              <div className="mt-0.5 text-xs text-slate-600 dark:text-slate-300">{s.effect}</div>
              {s.hint && <div className="mt-1 text-xs text-amber-800 dark:text-amber-300">{s.hint}</div>}
            </button>
          )
        })}
      </div>

      {/* Eigene Werte */}
      <details className="mt-4 rounded-lg border border-slate-200 p-3 dark:border-slate-700">
        <summary className="cursor-pointer text-sm font-semibold text-slate-900 focus-visible:outline-2 focus-visible:outline-blue-600 dark:text-slate-50">
          Eigene Werte einstellen
        </summary>
        <div className="mt-3 grid grid-cols-1 gap-3 md:grid-cols-2">
          {FIELDS.map((f) => {
            const c = custom[f.key]
            const id = `demo-${f.key}`
            return (
              <div key={f.key} className="flex items-center gap-3 text-sm">
                <label className="flex w-40 shrink-0 items-center gap-2 text-slate-800 dark:text-slate-200">
                  <input
                    type="checkbox"
                    checked={c.on}
                    onChange={(e) => setCustom({ ...custom, [f.key]: { ...c, on: e.target.checked } })}
                    className="h-4 w-4"
                  />
                  {f.label}
                </label>
                <input
                  id={id}
                  type="range"
                  aria-label={`${f.label} in ${f.unit}`}
                  min={f.min}
                  max={f.max}
                  step={f.step}
                  value={c.value}
                  disabled={!c.on}
                  onChange={(e) => setCustom({ ...custom, [f.key]: { ...c, value: Number(e.target.value) } })}
                  className="w-full accent-violet-600 disabled:opacity-40"
                />
                <output htmlFor={id} className="w-16 shrink-0 text-right tabular-nums text-slate-900 dark:text-slate-50">
                  {String(c.value).replace('.', ',')} {f.unit}
                </output>
              </div>
            )
          })}
        </div>
        <fieldset className="mt-3 text-sm">
          <legend className="mb-1 text-slate-800 dark:text-slate-200">Sensorausfall erzwingen</legend>
          <div className="flex flex-wrap gap-x-5 gap-y-1">
            {ERRORS.map((e) => (
              <label key={e} className="flex items-center gap-2 text-slate-700 dark:text-slate-300">
                <input
                  type="checkbox"
                  className="h-4 w-4"
                  checked={customErrors.includes(e)}
                  onChange={(ev) =>
                    setCustomErrors(ev.target.checked ? [...customErrors, e] : customErrors.filter((x) => x !== e))
                  }
                />
                {describeError(e)}
              </label>
            ))}
          </div>
        </fieldset>
        <button
          type="button"
          onClick={sendCustom}
          disabled={busy || customEmpty}
          className={`${btn} mt-3 border-violet-700 bg-violet-700 text-white hover:bg-violet-800 dark:border-violet-500 dark:bg-violet-600`}
        >
          Eigene Werte senden
        </button>
      </details>

      {/* Geraetebefehle */}
      <h3 className="mb-2 mt-4 text-sm font-semibold text-slate-900 dark:text-slate-50">Gerät</h3>
      <div className="flex flex-wrap gap-2">
        <button type="button" disabled={busy} onClick={() => command({ pump_run_s: 5 }, 'Jetzt gießen')} className={btnNeutral}>
          Jetzt gießen (5 s)
        </button>
        <button type="button" disabled={busy} onClick={() => command({ tank_refilled: true }, 'Tank aufgefüllt')} className={btnNeutral}>
          Tank aufgefüllt
        </button>
        <button type="button" disabled={busy} onClick={() => command({ identify: true }, 'Gerät finden')} className={btnNeutral}>
          Gerät finden (LED blinkt)
        </button>
        <button type="button" disabled={busy} onClick={() => command({ buzzer: true }, 'Piepen')} className={btnNeutral}>
          Piepen
        </button>
      </div>
      {pendingList.length > 0 && (
        <p className="mt-2 text-xs text-slate-600 dark:text-slate-300">Wartet auf das Gerät: {pendingList.join(', ')}</p>
      )}

      {message && (
        <p
          role={message.kind === 'error' ? 'alert' : undefined}
          className={`mt-3 text-sm ${message.kind === 'error' ? 'text-red-700 dark:text-red-300' : 'text-green-800 dark:text-green-300'}`}
        >
          {message.text}
        </p>
      )}
    </section>
  )
}
