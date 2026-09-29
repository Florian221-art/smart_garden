import type { ReactNode } from 'react'
import type { LucideIcon } from 'lucide-react'
import { ChevronDown, Table2 } from 'lucide-react'
import { useTranslation } from 'react-i18next'
import { Card } from './ui'

export interface LegendItem {
  label: string
  kind: 'line' | 'dot' | 'dash' | 'dots' | 'area'
  color: string
}

interface ChartCardProps {
  title: string
  icon?: LucideIcon
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
    return <span className="inline-block size-2.5 rounded-full" style={{ background: color }} />
  if (kind === 'dash')
    return <span className="inline-block w-4 border-t-2 border-dashed" style={{ borderColor: color }} />
  if (kind === 'dots')
    return <span className="inline-block w-4 border-t-2 border-dotted" style={{ borderColor: color }} />
  if (kind === 'area')
    return <span className="inline-block h-2.5 w-4 rounded-sm border border-line" style={{ background: color }} />
  return <span className="inline-block h-0.5 w-4 rounded" style={{ background: color }} />
}

export default function ChartCard({ title, icon: Icon, summary, ariaLabel, legend, children, table }: ChartCardProps) {
  const { t } = useTranslation()
  return (
    <Card as="section" className="min-w-0 p-4 sm:p-5">
      <div className="mb-3 flex flex-wrap items-start justify-between gap-x-4 gap-y-1">
        <h3 className="flex items-center gap-2 text-sm font-semibold text-fg">
          {Icon && <Icon aria-hidden="true" className="size-4 text-muted" />}
          {title}
        </h3>
        {summary && <span className="text-sm tabular-nums text-fg-2">{summary}</span>}
      </div>
      {legend && legend.length > 0 && (
        <ul className="mb-3 flex flex-wrap gap-x-4 gap-y-1 text-xs text-fg-2">
          {legend.map((l) => (
            <li key={l.label} className="flex items-center gap-1.5">
              <Swatch kind={l.kind} color={l.color} />
              {l.label}
            </li>
          ))}
        </ul>
      )}
      <div role="img" aria-label={ariaLabel} className="h-52 w-full sm:h-56">
        {children}
      </div>
      {table && table.rows.length > 0 && (
        <details className="group mt-2 text-xs text-fg-2">
          <summary className="inline-flex min-h-11 cursor-pointer select-none list-none items-center gap-1.5 rounded-lg font-medium text-muted hover:text-fg [&::-webkit-details-marker]:hidden">
            <Table2 aria-hidden="true" className="size-4" />
            {t('chart.showTable')}
            <ChevronDown aria-hidden="true" className="size-4 transition-transform group-open:rotate-180" />
          </summary>
          <div className="mt-1 max-h-60 overflow-auto rounded-lg border border-line">
            <table className="w-full text-left tabular-nums">
              <thead className="sticky top-0 bg-surface-2">
                <tr>
                  {table.head.map((h) => (
                    <th key={h} className="px-3 py-2 font-medium text-fg">
                      {h}
                    </th>
                  ))}
                </tr>
              </thead>
              <tbody>
                {table.rows.map((r, i) => (
                  <tr key={i} className="border-t border-line">
                    {r.map((c, j) => (
                      <td key={j} className="px-3 py-1.5">
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
    </Card>
  )
}
