import {
  Area,
  AreaChart,
  CartesianGrid,
  Line,
  LineChart,
  ReferenceArea,
  ReferenceLine,
  ResponsiveContainer,
  Tooltip,
  XAxis,
  YAxis,
} from 'recharts'
import type { HistoryPoint } from '../types'
import { describeError } from '../errorMessages'

export type ChartPoint = HistoryPoint & { ts: number }

export type NumericKey =
  | 'soil_moisture_pct'
  | 'light_pct'
  | 'air_temp_c'
  | 'air_humidity_pct'
  | 'water_level_pct'

export interface RefLine {
  y: number
  label: string
  color: string
}

interface Props {
  data: ChartPoint[]
  dataKey: NumericKey
  kind: 'line' | 'area'
  unit: string
  digits?: number
  xDomain: [number, number]
  yDomain?: [number | string, number | string]
  refLines?: RefLine[]
  demoRanges?: [number, number][]
  /** Punkte mit automatischer Bewaesserung als Marker zeigen */
  markWatering?: boolean
  tickFormat: (ts: number) => string
}

const AXIS_TICK = { fill: 'var(--viz-axis)', fontSize: 11 }

function fmtNum(v: number | null | undefined, digits: number): string {
  return v === null || v === undefined ? '–' : v.toFixed(digits).replace('.', ',')
}

export default function TimeSeriesChart({
  data,
  dataKey,
  kind,
  unit,
  digits = 1,
  xDomain,
  yDomain,
  refLines = [],
  demoRanges = [],
  markWatering = false,
  tickFormat,
}: Props) {
  const common = {
    data,
    margin: { top: 8, right: 12, bottom: 0, left: -12 },
  }

  const xAxis = (
    <XAxis
      dataKey="ts"
      type="number"
      scale="time"
      domain={xDomain}
      tickFormatter={tickFormat}
      tick={AXIS_TICK}
      tickLine={false}
      axisLine={{ stroke: 'var(--viz-grid)' }}
      minTickGap={40}
    />
  )
  const yAxis = (
    <YAxis
      domain={yDomain ?? ['auto', 'auto']}
      tick={AXIS_TICK}
      tickLine={false}
      axisLine={false}
      width={44}
      tickFormatter={(v: number) => `${v}`}
    />
  )
  const grid = <CartesianGrid vertical={false} stroke="var(--viz-grid)" strokeWidth={1} />

  const tooltip = (
    <Tooltip
      cursor={{ stroke: 'var(--viz-axis)', strokeWidth: 1 }}
      content={({ active, payload }) => {
        if (!active || !payload || payload.length === 0) return null
        const p = payload[0].payload as ChartPoint
        const time = new Date(p.ts).toLocaleString('de-DE', {
          weekday: 'short',
          hour: '2-digit',
          minute: '2-digit',
        })
        return (
          <div className="rounded-lg border border-slate-300 bg-white px-3 py-2 text-xs shadow-md dark:border-slate-600 dark:bg-slate-800">
            <div className="text-slate-500 dark:text-slate-400">{time}</div>
            <div className="text-sm font-semibold text-slate-900 dark:text-slate-50">
              {fmtNum(p[dataKey], digits)} {unit}
            </div>
            {p.auto_water_count > 0 && (
              <div className="text-slate-700 dark:text-slate-200">
                Automatisch gegossen ({p.auto_water_count}×, {fmtNum(p.water_ml, 0)} ml)
              </div>
            )}
            {p.demo && <div className="font-medium" style={{ color: 'var(--viz-demo-text)' }}>DEMO-Werte</div>}
            {p.errors.map((e) => (
              <div key={e} className="text-red-700 dark:text-red-300">
                ⚠ {describeError(e)}
              </div>
            ))}
          </div>
        )
      }}
    />
  )

  const demoAreas = demoRanges.map(([a, b]) => (
    <ReferenceArea
      key={`demo-${a}`}
      x1={a}
      x2={b}
      fill="var(--viz-demo)"
      fillOpacity={1}
      ifOverflow="extendDomain"
    />
  ))

  const refs = refLines.map((r) => (
    <ReferenceLine
      key={r.label}
      y={r.y}
      stroke={r.color}
      strokeDasharray="4 4"
      strokeWidth={1.5}
      ifOverflow="extendDomain"
    />
  ))

  const waterDot = (props: unknown) => {
    const { cx, cy, payload, index } = props as { cx?: number; cy?: number; payload?: ChartPoint; index?: number }
    if (!markWatering || !payload || payload.auto_water_count === 0 || cx == null || cy == null)
      return <g key={`d-${index}`} />
    return (
      <circle
        key={`d-${index}`}
        cx={cx}
        cy={cy}
        r={5}
        fill="var(--series-2)"
        stroke="var(--viz-surface)"
        strokeWidth={2}
      />
    )
  }

  let chart
  if (kind === 'area') {
    chart = (
      <AreaChart {...common}>
        {grid}
        {demoAreas}
        {xAxis}
        {yAxis}
        {tooltip}
        {refs}
        <Area
          type="monotone"
          dataKey={dataKey}
          stroke="var(--series-1)"
          strokeWidth={2}
          fill="var(--series-1)"
          fillOpacity={0.12}
          connectNulls={false}
          isAnimationActive={false}
          activeDot={{ r: 4, stroke: 'var(--viz-surface)', strokeWidth: 2 }}
        />
      </AreaChart>
    )
  } else {
    chart = (
      <LineChart {...common}>
        {grid}
        {demoAreas}
        {xAxis}
        {yAxis}
        {tooltip}
        {refs}
        <Line
          type="monotone"
          dataKey={dataKey}
          stroke="var(--series-1)"
          strokeWidth={2}
          dot={markWatering ? waterDot : false}
          connectNulls={false}
          isAnimationActive={false}
          activeDot={{ r: 4, stroke: 'var(--viz-surface)', strokeWidth: 2 }}
        />
      </LineChart>
    )
  }

  return (
    <ResponsiveContainer width="100%" height="100%">
      {chart}
    </ResponsiveContainer>
  )
}
