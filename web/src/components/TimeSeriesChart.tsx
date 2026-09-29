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
import { CircleAlert, FlaskConical, ShowerHead } from 'lucide-react'

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
  /** Strichmuster, damit sich mehrere Grenzlinien ohne Farbe unterscheiden lassen */
  dash?: string
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
          <div className="rounded-xl border border-line bg-surface px-3 py-2 text-xs shadow-lg">
            <div className="text-muted">{time}</div>
            <div className="text-sm font-semibold text-fg">
              {fmtNum(p[dataKey], digits)} {unit}
            </div>
            {p.auto_water_count > 0 && (
              <div className="flex items-center gap-1 text-fg-2">
                <ShowerHead aria-hidden="true" className="size-3.5" />
                Automatisch gegossen ({p.auto_water_count}×, {fmtNum(p.water_ml, 0)} ml)
              </div>
            )}
            {p.demo && (
              <div className="flex items-center gap-1 font-medium text-fg-2">
                <FlaskConical aria-hidden="true" className="size-3.5" />
                Demo-Werte
              </div>
            )}
            {p.errors.map((e) => (
              <div key={e} className="flex items-center gap-1 text-danger">
                <CircleAlert aria-hidden="true" className="size-3.5 shrink-0" />
                {describeError(e)}
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
      strokeDasharray={r.dash ?? '4 4'}
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
        fill="var(--viz-mark)"
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
          stroke="var(--viz-series)"
          strokeWidth={2}
          fill="var(--viz-series)"
          fillOpacity={0.1}
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
          stroke="var(--viz-series)"
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
