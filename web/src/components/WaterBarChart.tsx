import { Bar, BarChart, CartesianGrid, ResponsiveContainer, Tooltip, XAxis, YAxis } from 'recharts'

export interface WaterBar {
  key: number
  label: string
  longLabel: string
  water_ml: number
  waterings: number
}

const AXIS_TICK = { fill: 'var(--viz-axis)', fontSize: 11 }

export default function WaterBarChart({ data }: { data: WaterBar[] }) {
  return (
    <ResponsiveContainer width="100%" height="100%">
      <BarChart data={data} margin={{ top: 8, right: 12, bottom: 0, left: -12 }} barCategoryGap={2}>
        <CartesianGrid vertical={false} stroke="var(--viz-grid)" strokeWidth={1} />
        <XAxis
          dataKey="label"
          tick={AXIS_TICK}
          tickLine={false}
          axisLine={{ stroke: 'var(--viz-grid)' }}
          interval="preserveStartEnd"
          minTickGap={24}
        />
        <YAxis tick={AXIS_TICK} tickLine={false} axisLine={false} width={44} allowDecimals={false} />
        <Tooltip
          cursor={{ fill: 'var(--viz-grid)', opacity: 0.5 }}
          content={({ active, payload }) => {
            if (!active || !payload || payload.length === 0) return null
            const b = payload[0].payload as WaterBar
            return (
              <div className="rounded-lg border border-slate-300 bg-white px-3 py-2 text-xs shadow-md dark:border-slate-600 dark:bg-slate-800">
                <div className="text-slate-500 dark:text-slate-400">{b.longLabel}</div>
                <div className="text-sm font-semibold text-slate-900 dark:text-slate-50">{Math.round(b.water_ml)} ml</div>
                {b.waterings > 0 && (
                  <div className="text-slate-700 dark:text-slate-200">{b.waterings}× automatisch gegossen</div>
                )}
              </div>
            )
          }}
        />
        <Bar dataKey="water_ml" fill="var(--series-1)" radius={[4, 4, 0, 0]} isAnimationActive={false} />
      </BarChart>
    </ResponsiveContainer>
  )
}
