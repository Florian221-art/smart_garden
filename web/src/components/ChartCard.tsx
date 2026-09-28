import type { ReactNode } from 'react'

export interface LegendItem {
  label: string
  kind: 'line' | 'dot' | 'dash' | 'area'
  color: string
}

interface ChartCardProps {
  title: string
  summary?: string
  /** Kurzbeschreibung fuer Screenreader (Diagramm ist role="img") */
  ariaLabel: string
  legend?: LegendItem[]
  children: ReactNode
  /** Tabellenansicht als Alternative zum Diagramm (WCAG: nie nur Grafik) */
  table?: { head: string[]; rows: (string | number)[][] }
}

function Swatch({ kind, color }: { kind: LegendItem['kind']; color: string }) {
  if (kind === 'dot')
    return <span className="inline-block h-2.5 w-2.5 rounded-full" style={{ background: color }} />
  if (kind === 'dash')
    return <span className="inline-block w-4 border-t-2 border-dashed" style={{ borderColor: color }} />
  if (kind === 'area')
    return <span className="inline-block h-2.5 w-4 rounded-sm" style={{ background: color }} />
  return <span className="inline-block h-0.5 w-4 rounded" style={{ background: color }} />
}

export default function ChartCard({ title, summary, ariaLabel, legend, children, table }: ChartCardProps) {
  return (
    <section className="min-w-0 rounded-xl border border-slate-300/60 bg-white p-3 shadow-sm sm:p-4 dark:border-slate-700 dark:bg-slate-900">
      <div className="mb-2 flex flex-wrap items-baseline justify-between gap-x-4 gap-y-1">
        <h3 className="text-sm font-semibold text-slate-900 dark:text-slate-50">{title}</h3>
        {summary && <span className="text-sm text-slate-600 dark:text-slate-300">{summary}</span>}
      </div>
      {legend && legend.length > 0 && (
        <ul className="mb-2 flex flex-wrap gap-x-4 gap-y-1 text-xs text-slate-600 dark:text-slate-300">
          {legend.map((l) => (
            <li key={l.label} className="flex items-center gap-1.5">
              <Swatch kind={l.kind} color={l.color} />
              {l.label}
            </li>
          ))}
        </ul>
      )}
      <div role="img" aria-label={ariaLabel} className="h-56 w-full">
        {children}
      </div>
      {table && table.rows.length > 0 && (
        <details className="mt-2 text-xs text-slate-600 dark:text-slate-300">
          <summary className="flex min-h-11 cursor-pointer select-none items-center rounded focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-blue-600">
            Als Tabelle anzeigen
          </summary>
          <div className="mt-2 max-h-60 overflow-auto">
            <table className="w-full text-left tabular-nums">
              <thead className="sticky top-0 bg-white dark:bg-slate-900">
                <tr>
                  {table.head.map((h) => (
                    <th key={h} className="py-1 pr-3 font-medium">
                      {h}
                    </th>
                  ))}
                </tr>
              </thead>
              <tbody>
                {table.rows.map((r, i) => (
                  <tr key={i} className="border-t border-slate-200 dark:border-slate-800">
                    {r.map((c, j) => (
                      <td key={j} className="py-1 pr-3">
                        {c}
                      </td>
                    ))}
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </details>
      )}
    </section>
  )
}
