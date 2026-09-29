import type { ReactNode } from 'react'
import type { LucideIcon } from 'lucide-react'
import { FlaskConical } from 'lucide-react'
import { Badge, Card, Meter, StatusLine, type Tone } from './ui'

interface StatTileProps {
  icon: LucideIcon
  label: string
  /** Hauptwert ohne Einheit, z. B. "42" - null = kein Wert */
  value: string | null
  unit?: string
  /** Verstaendliche Einordnung, z. B. "Gut feucht" */
  status?: { tone: Tone; text: string }
  /** 0-100: zeigt einen Balken unter dem Wert */
  meter?: number | null
  /** Kleiner Zusatz, z. B. "ca. 850 ml" */
  hint?: ReactNode
  /** Wert ist gerade per Demo-Modus ueberschrieben */
  demo?: boolean
}

export default function StatTile({ icon: Icon, label, value, unit, status, meter, hint, demo = false }: StatTileProps) {
  return (
    <Card as="article" className="flex min-w-0 flex-col gap-3 p-4" aria-label={label}>
      <div className="flex items-center justify-between gap-2">
        <div className="flex min-w-0 items-center gap-2 sm:gap-2.5">
          <span className="grid size-8 shrink-0 place-items-center rounded-lg bg-surface-2 text-fg-2 sm:size-9 sm:rounded-xl">
            <Icon aria-hidden="true" className="size-[18px]" />
          </span>
          <h3 className="min-w-0 text-sm font-medium leading-tight text-fg-2 [overflow-wrap:anywhere]">{label}</h3>
        </div>
        {demo && (
          <Badge icon={FlaskConical}>
            Demo
          </Badge>
        )}
      </div>

      <div className="flex items-baseline gap-1">
        <span className="text-3xl font-semibold tracking-tight text-fg">{value ?? '–'}</span>
        {value !== null && unit && <span className="text-base font-medium text-muted">{unit}</span>}
      </div>

      {meter !== undefined && <Meter value={meter} tone={status?.tone === 'neutral' ? 'ok' : status?.tone} label={`${label} ${value ?? 'unbekannt'} ${unit ?? ''}`} />}

      <div className="mt-auto flex flex-col gap-1">
        {status && <StatusLine tone={status.tone}>{status.text}</StatusLine>}
        {hint && <p className="text-xs text-muted">{hint}</p>}
      </div>
    </Card>
  )
}
