import { useEffect, useMemo, useState } from 'react'
import type { HistoryResponse } from './types'
import ChartCard from './components/ChartCard'
import StatTile from './components/StatTile'
import TimeSeriesChart, { type ChartPoint, type NumericKey } from './components/TimeSeriesChart'
import WaterBarChart, { type WaterBar } from './components/WaterBarChart'

type RangeKey = '1h' | '24h' | '7d'
const RANGES: { key: RangeKey; label: string; ms: number; group: 'min5' | 'hour' | 'day'; groupLabel: string }[] = [
  { key: '1h', label: '1 Stunde', ms: 3600_000, group: 'min5', groupLabel: 'pro 5 Minuten' },
  { key: '24h', label: '24 Stunden', ms: 24 * 3600_000, group: 'hour', groupLabel: 'pro Stunde' },
  { key: '7d', label: '7 Tage', ms: 7 * 24 * 3600_000, group: 'day', groupLabel: 'pro Tag' },
]
const REFRESH_MS = 30_000

function fmt(v: number | null | undefined, digits = 1): string {
  return v === null || v === undefined ? '–' : v.toFixed(digits).replace('.', ',')
}

/** Zusammenhaengende Demo-Buckets zu Zeitspannen [start, ende] zusammenfassen */
function demoRanges(points: ChartPoint[], bucketMs: number): [number, number][] {
  const out: [number, number][] = []
  for (const p of points) {
    if (!p.demo) continue
    const last = out[out.length - 1]
    if (last && p.ts <= last[1]) last[1] = p.ts + bucketMs
    else out.push([p.ts, p.ts + bucketMs])
  }
  return out
}

/** Wassermengen fuer Balken zusammenfassen: lueckenlos, auch Abschnitte ohne Giessen (= 0 ml) */
function waterBars(points: ChartPoint[], from: number, to: number, group: 'min5' | 'hour' | 'day'): WaterBar[] {
  const floor = (ts: number) => {
    const d = new Date(ts)
    if (group === 'day') d.setHours(0, 0, 0, 0)
    else if (group === 'hour') d.setMinutes(0, 0, 0)
    else d.setMinutes(Math.floor(d.getMinutes() / 5) * 5, 0, 0)
    return d.getTime()
  }
  const step = (ts: number) => {
    const d = new Date(ts)
    if (group === 'day') d.setDate(d.getDate() + 1)
    else if (group === 'hour') d.setHours(d.getHours() + 1)
    else d.setMinutes(d.getMinutes() + 5)
    return d.getTime()
  }
  const bars = new Map<number, WaterBar>()
  for (let t = floor(from); t <= to; t = step(t)) {
    const d = new Date(t)
    bars.set(t, {
      key: t,
      label:
        group === 'day'
          ? d.toLocaleDateString('de-DE', { weekday: 'short' })
          : d.toLocaleTimeString('de-DE', { hour: '2-digit', minute: '2-digit' }),
      longLabel:
        group === 'day'
          ? d.toLocaleDateString('de-DE', { weekday: 'long', day: '2-digit', month: '2-digit' })
          : `${d.toLocaleTimeString('de-DE', { hour: '2-digit', minute: '2-digit' })}–${new Date(step(t)).toLocaleTimeString('de-DE', { hour: '2-digit', minute: '2-digit' })}`,
      water_ml: 0,
      waterings: 0,
    })
  }
  for (const p of points) {
    const b = bars.get(floor(p.ts))
    if (b) {
      b.water_ml += p.water_ml
      b.waterings += p.auto_water_count
    }
  }
  return [...bars.values()]
}

function avg(points: ChartPoint[], key: NumericKey): number | null {
  const vals = points.map((p) => p[key]).filter((v): v is number => v !== null)
  return vals.length ? vals.reduce((a, b) => a + b, 0) / vals.length : null
}

function minMax(points: ChartPoint[], key: NumericKey): [number, number] | null {
  const vals = points.map((p) => p[key]).filter((v): v is number => v !== null)
  return vals.length ? [Math.min(...vals), Math.max(...vals)] : null
}

export default function HistoryCharts({ deviceId }: { deviceId: string }) {
  const [range, setRange] = useState<RangeKey>('24h')
  const [data, setData] = useState<HistoryResponse | null>(null)
  const [error, setError] = useState<string | null>(null)

  useEffect(() => {
    let cancelled = false
    const ms = RANGES.find((r) => r.key === range)!.ms
    async function load() {
      const from = new Date(Date.now() - ms).toISOString()
      try {
        const res = await fetch(`/api/v1/devices/${deviceId}/readings?from=${encodeURIComponent(from)}`)
        if (!res.ok) throw new Error(`HTTP ${res.status}`)
        const json: HistoryResponse = await res.json()
        if (!cancelled) {
          setData(json)
          setError(null)
        }
      } catch {
        if (!cancelled) setError('Verlauf konnte nicht geladen werden.')
      }
    }
    load()
    const id = setInterval(load, REFRESH_MS)
    return () => {
      cancelled = true
      clearInterval(id)
    }
  }, [deviceId, range])

  const view = useMemo(() => {
    if (!data) return null
    const bucketMs = data.bucket_s * 1000
    const points: ChartPoint[] = data.points.map((p) => ({ ...p, ts: new Date(p.t).getTime() }))
    const xDomain: [number, number] = [new Date(data.from).getTime(), new Date(data.to).getTime()]
    const multiDay = xDomain[1] - xDomain[0] > 36 * 3600_000
    const tickFormat = (ts: number) =>
      new Date(ts).toLocaleString('de-DE', multiDay ? { weekday: 'short', day: '2-digit' } : { hour: '2-digit', minute: '2-digit' })
    const rowTime = (ts: number) =>
      new Date(ts).toLocaleString('de-DE', { weekday: 'short', hour: '2-digit', minute: '2-digit' })
    const tableFor = (key: NumericKey, unit: string, digits = 1) => ({
      head: ['Zeit', `Wert (${unit})`, 'Hinweis'],
      rows: points.map((p) => [
        rowTime(p.ts),
        fmt(p[key], digits),
        [p.demo ? 'DEMO' : '', p.auto_water_count ? `gegossen ${p.auto_water_count}×` : '', ...p.errors]
          .filter(Boolean)
          .join(', '),
      ]),
    })
    const totalWater = points.reduce((a, p) => a + p.water_ml, 0)
    const waterings = points.reduce((a, p) => a + p.auto_water_count, 0)
    const rangeCfg = RANGES.find((r) => r.key === range)!
    return {
      bars: waterBars(points, xDomain[0], xDomain[1], rangeCfg.group),
      barsLabel: rangeCfg.groupLabel,
      bucketMs,
      points,
      xDomain,
      tickFormat,
      tableFor,
      demo: demoRanges(points, bucketMs),
      totalWater,
      waterings,
      cfg: data.config,
    }
  }, [data, range])

  const rangeLabel = RANGES.find((r) => r.key === range)!.label

  return (
    <section aria-labelledby="verlauf-titel" className="viz-root mt-10">
      <div className="mb-4 flex flex-wrap items-center justify-between gap-3">
        <h2 id="verlauf-titel" className="text-lg font-bold text-slate-900 dark:text-slate-50">
          Verlauf
        </h2>
        <div role="group" aria-label="Zeitraum" className="inline-flex rounded-lg border border-slate-300 p-0.5 dark:border-slate-700">
          {RANGES.map((r) => (
            <button
              key={r.key}
              type="button"
              aria-pressed={range === r.key}
              onClick={() => setRange(r.key)}
              className={`rounded-md px-3 py-1 text-sm focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-blue-600 ${
                range === r.key
                  ? 'bg-slate-900 text-white dark:bg-slate-100 dark:text-slate-900'
                  : 'text-slate-700 hover:bg-slate-100 dark:text-slate-300 dark:hover:bg-slate-800'
              }`}
            >
              {r.label}
            </button>
          ))}
        </div>
      </div>

      {error && (
        <div role="alert" className="mb-4 rounded-lg border border-amber-500/50 bg-amber-50 p-3 text-sm text-amber-800 dark:bg-amber-950/40 dark:text-amber-200">
          {error}
        </div>
      )}

      {view && view.points.length === 0 && (
        <p className="rounded-lg border border-slate-300/60 bg-white p-4 text-sm text-slate-600 dark:border-slate-700 dark:bg-slate-900 dark:text-slate-300">
          Keine Messwerte in den letzten {rangeLabel}.
        </p>
      )}

      {view && view.points.length > 0 && (
        <>
          <div className="mb-4 grid grid-cols-1 gap-4 sm:grid-cols-3">
            <StatTile
              label={`Wasserverbrauch (${rangeLabel})`}
              value={`${fmt(view.totalWater, 0)} ml`}
              hint={`aus Pumpenlaufzeit × ${view.cfg.pump_flow_ml_per_s} ml/s (Schätzung)`}
            />
            <StatTile
              label="Automatisch gegossen"
              value={`${view.waterings}×`}
              hint={view.cfg.auto_water ? 'Auto-Bewässerung ist an' : 'Auto-Bewässerung ist aus'}
            />
            <StatTile
              label="Ø Bodenfeuchte"
              value={`${fmt(avg(view.points, 'soil_moisture_pct'))} %`}
              hint={`Gießschwelle ${view.cfg.moisture_min_pct} %, Ziel ${view.cfg.moisture_target_pct} %`}
            />
          </div>

          <div className="grid grid-cols-1 gap-4 lg:grid-cols-2">
            <ChartCard
              title="Bodenfeuchte"
              summary={(() => {
                const mm = minMax(view.points, 'soil_moisture_pct')
                return mm ? `${fmt(mm[0])}–${fmt(mm[1])} %` : undefined
              })()}
              ariaLabel={`Bodenfeuchte der letzten ${rangeLabel} in Prozent, ${view.waterings} automatische Bewässerungen, Gießschwelle ${view.cfg.moisture_min_pct} Prozent.`}
              legend={[
                { label: 'Bodenfeuchte', kind: 'line', color: 'var(--series-1)' },
                { label: 'Automatisch gegossen', kind: 'dot', color: 'var(--series-2)' },
                { label: `Gießschwelle ${view.cfg.moisture_min_pct} %`, kind: 'dash', color: 'var(--status-warning)' },
                { label: `Ziel ${view.cfg.moisture_target_pct} %`, kind: 'dash', color: 'var(--viz-muted)' },
                ...(view.demo.length ? [{ label: 'Demo-Zeitraum', kind: 'area' as const, color: 'var(--viz-demo)' }] : []),
              ]}
              table={view.tableFor('soil_moisture_pct', '%')}
            >
              <TimeSeriesChart
                data={view.points}
                dataKey="soil_moisture_pct"
                kind="line"
                unit="%"
                xDomain={view.xDomain}
                yDomain={[0, 100]}
                tickFormat={view.tickFormat}
                demoRanges={view.demo}
                markWatering
                refLines={[
                  { y: view.cfg.moisture_min_pct, label: `Gießschwelle ${view.cfg.moisture_min_pct} %`, color: 'var(--status-warning)' },
                  { y: view.cfg.moisture_target_pct, label: `Ziel ${view.cfg.moisture_target_pct} %`, color: 'var(--viz-muted)' },
                ]}
              />
            </ChartCard>

            <ChartCard
              title="Wassertank (Schätzung)"
              summary={`aktuell ${fmt(view.points[view.points.length - 1].water_level_pct, 0)} %`}
              ariaLabel={`Geschätzter Tankfüllstand der letzten ${rangeLabel}, Warnschwelle ${view.cfg.tank_low_pct} Prozent.`}
              legend={[{ label: `Warnung unter ${view.cfg.tank_low_pct} %`, kind: 'dash', color: 'var(--status-warning)' }]}
              table={view.tableFor('water_level_pct', '%', 0)}
            >
              <TimeSeriesChart
                data={view.points}
                dataKey="water_level_pct"
                kind="area"
                unit="%"
                digits={0}
                xDomain={view.xDomain}
                yDomain={[0, 100]}
                tickFormat={view.tickFormat}
                demoRanges={view.demo}
                refLines={[{ y: view.cfg.tank_low_pct, label: `Warnung unter ${view.cfg.tank_low_pct} %`, color: 'var(--status-warning)' }]}
              />
            </ChartCard>

            <ChartCard
              title={`Wasserverbrauch ${view.barsLabel}`}
              summary={`${fmt(view.totalWater, 0)} ml gesamt`}
              ariaLabel={`Gepumpte Wassermenge ${view.barsLabel}, insgesamt ${Math.round(view.totalWater)} Milliliter.`}
              table={{
                head: ['Zeitraum', 'Wasser (ml)', 'Gegossen'],
                rows: view.bars.filter((b) => b.water_ml > 0).map((b) => [b.longLabel, fmt(b.water_ml, 0), `${b.waterings}×`]),
              }}
            >
              <WaterBarChart data={view.bars} />
            </ChartCard>

            <ChartCard
              title="Lufttemperatur"
              summary={(() => {
                const mm = minMax(view.points, 'air_temp_c')
                return mm ? `${fmt(mm[0])}–${fmt(mm[1])} °C` : undefined
              })()}
              ariaLabel={`Lufttemperatur der letzten ${rangeLabel} in Grad Celsius.`}
              table={view.tableFor('air_temp_c', '°C')}
            >
              <TimeSeriesChart
                data={view.points}
                dataKey="air_temp_c"
                kind="line"
                unit="°C"
                xDomain={view.xDomain}
                yDomain={[(min: number) => Math.floor(min - 2), (max: number) => Math.ceil(max + 2)] as unknown as [number, number]}
                tickFormat={view.tickFormat}
                demoRanges={view.demo}
              />
            </ChartCard>

            <ChartCard
              title="Luftfeuchte"
              summary={`Ø ${fmt(avg(view.points, 'air_humidity_pct'), 0)} %`}
              ariaLabel={`Relative Luftfeuchte der letzten ${rangeLabel} in Prozent.`}
              table={view.tableFor('air_humidity_pct', '%', 0)}
            >
              <TimeSeriesChart
                data={view.points}
                dataKey="air_humidity_pct"
                kind="line"
                unit="%"
                digits={0}
                xDomain={view.xDomain}
                yDomain={[0, 100]}
                tickFormat={view.tickFormat}
                demoRanges={view.demo}
              />
            </ChartCard>

            <ChartCard
              title="Licht"
              summary="0 % = dunkel"
              ariaLabel={`Helligkeit der letzten ${rangeLabel} in Prozent, zeigt den Tag-Nacht-Verlauf.`}
              table={view.tableFor('light_pct', '%', 0)}
            >
              <TimeSeriesChart
                data={view.points}
                dataKey="light_pct"
                kind="area"
                unit="%"
                digits={0}
                xDomain={view.xDomain}
                yDomain={[0, 100]}
                tickFormat={view.tickFormat}
                demoRanges={view.demo}
              />
            </ChartCard>
          </div>
          <p className="mt-3 text-xs text-slate-500 dark:text-slate-400">
            Werte sind Mittelwerte je {Math.round(view.bucketMs / 60000) || 1} min. Lücken in einer Linie = Sensor
            ausgefallen. Aktualisiert alle 30 s.
          </p>
        </>
      )}
    </section>
  )
}
