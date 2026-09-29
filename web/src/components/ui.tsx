// Kleine Grundbausteine, damit alle Teile gleich aussehen (Karten, Knoepfe, Umschalter, Status).
import type { ButtonHTMLAttributes, ReactNode } from 'react'
import type { LucideIcon } from 'lucide-react'
import { CircleAlert, CircleCheck, Info, TriangleAlert } from 'lucide-react'

export type Tone = 'ok' | 'warn' | 'danger' | 'neutral'

export function Card({ children, className = '', as: Tag = 'div', ...rest }: {
  children: ReactNode
  className?: string
  as?: 'div' | 'section' | 'article'
} & Record<string, unknown>) {
  return (
    <Tag className={`rounded-2xl border border-line bg-surface shadow-card ${className}`} {...rest}>
      {children}
    </Tag>
  )
}

type Variant = 'primary' | 'secondary' | 'ghost'
const variantClass: Record<Variant, string> = {
  primary: 'border-transparent bg-accent text-accent-fg hover:bg-accent-hover',
  secondary: 'border-line bg-surface text-fg hover:bg-surface-2',
  ghost: 'border-transparent bg-transparent text-fg-2 hover:bg-surface-2 hover:text-fg',
}

export function Button({
  variant = 'secondary',
  icon: Icon,
  children,
  className = '',
  ...rest
}: ButtonHTMLAttributes<HTMLButtonElement> & { variant?: Variant; icon?: LucideIcon }) {
  return (
    <button
      type="button"
      className={`inline-flex min-h-11 items-center justify-center gap-2 rounded-xl border px-3.5 text-sm font-medium transition-colors disabled:cursor-not-allowed disabled:opacity-50 ${variantClass[variant]} ${className}`}
      {...rest}
    >
      {Icon && <Icon aria-hidden="true" className="size-4 shrink-0" />}
      {children}
    </button>
  )
}

/** Auswahl aus wenigen Optionen (Zeitraum, Dauer, Hell/Dunkel) */
export function Segmented<T extends string | number>({
  label,
  options,
  value,
  onChange,
  size = 'md',
}: {
  label: string
  options: { value: T; label: ReactNode; title?: string }[]
  value: T
  onChange: (v: T) => void
  size?: 'sm' | 'md'
}) {
  return (
    <div role="group" aria-label={label} className="inline-flex rounded-xl bg-surface-2 p-1">
      {options.map((o) => {
        const active = o.value === value
        return (
          <button
            key={String(o.value)}
            type="button"
            title={o.title}
            aria-label={o.title}
            aria-pressed={active}
            onClick={() => onChange(o.value)}
            className={`inline-flex items-center justify-center gap-1.5 rounded-lg font-medium transition-colors ${
              size === 'sm' ? 'min-h-9 min-w-9 px-2 text-sm' : 'min-h-9 px-3 text-sm'
            } ${active ? 'bg-surface text-fg shadow-card' : 'text-fg-2 hover:text-fg'}`}
          >
            {o.label}
          </button>
        )
      })}
    </div>
  )
}

const toneIcon: Record<Tone, LucideIcon> = {
  ok: CircleCheck,
  warn: TriangleAlert,
  danger: CircleAlert,
  neutral: Info,
}
const toneText: Record<Tone, string> = {
  ok: 'text-accent',
  warn: 'text-warn',
  danger: 'text-danger',
  neutral: 'text-muted',
}
export const toneSoft: Record<Tone, string> = {
  ok: 'bg-accent-soft text-accent-ink',
  warn: 'bg-warn-soft text-warn',
  danger: 'bg-danger-soft text-danger',
  neutral: 'bg-surface-2 text-fg-2',
}

/** Zustand immer als Symbol + Text, nie nur als Farbe (WCAG) */
export function StatusLine({ tone, children }: { tone: Tone; children: ReactNode }) {
  const Icon = toneIcon[tone]
  return (
    <span className={`inline-flex items-start gap-1.5 text-sm font-medium ${toneText[tone]}`}>
      <Icon aria-hidden="true" className="mt-0.5 size-4 shrink-0" />
      <span className="min-w-0">{children}</span>
    </span>
  )
}

export function Badge({ tone = 'neutral', icon: Icon, children }: { tone?: Tone; icon?: LucideIcon; children: ReactNode }) {
  return (
    <span className={`inline-flex items-center gap-1 rounded-full px-2 py-0.5 text-xs font-semibold ${toneSoft[tone]}`}>
      {Icon && <Icon aria-hidden="true" className="size-3.5" />}
      {children}
    </span>
  )
}

/** Fortschrittsbalken (z. B. Bodenfeuchte, Tank) - der Wert steht zusaetzlich immer als Zahl daneben */
export function Meter({ value, tone = 'ok', label }: { value: number | null; tone?: Tone; label: string }) {
  const pct = value === null ? 0 : Math.max(0, Math.min(100, value))
  const fill = tone === 'danger' ? 'bg-danger' : tone === 'warn' ? 'bg-warn' : 'bg-accent'
  return (
    <div
      role="meter"
      aria-label={label}
      aria-valuemin={0}
      aria-valuemax={100}
      aria-valuenow={value === null ? undefined : Math.round(pct)}
      className="h-1.5 w-full overflow-hidden rounded-full bg-surface-2"
    >
      <div className={`h-full rounded-full ${fill}`} style={{ width: `${pct}%` }} />
    </div>
  )
}

export function SectionTitle({ id, icon: Icon, children, right }: {
  id?: string
  icon?: LucideIcon
  children: ReactNode
  right?: ReactNode
}) {
  return (
    <div className="mb-3 flex flex-wrap items-center justify-between gap-3">
      <h2 id={id} className="flex items-center gap-2 text-base font-semibold text-fg">
        {Icon && <Icon aria-hidden="true" className="size-5 text-muted" />}
        {children}
      </h2>
      {right}
    </div>
  )
}
