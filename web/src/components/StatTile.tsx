interface StatTileProps {
  label: string
  value: string
  hint?: string
  tone?: 'ok' | 'warn' | 'critical' | 'neutral'
  /** Wert ist gerade per Demo-Modus ueberschrieben */
  demo?: boolean
}

// Farbe ist immer NUR eine Zusatzinformation - Wert/Text stehen fuer sich
// (WCAG-Anforderung aus plan-webserver.md Abschnitt 6: nie nur Farbe).
const toneClasses: Record<NonNullable<StatTileProps['tone']>, string> = {
  ok: 'border-green-600/40 bg-green-50 dark:bg-green-950/40',
  warn: 'border-amber-600/40 bg-amber-50 dark:bg-amber-950/40',
  critical: 'border-red-600/40 bg-red-50 dark:bg-red-950/40',
  neutral: 'border-slate-300/60 bg-white dark:bg-slate-900',
}

export default function StatTile({ label, value, hint, tone = 'neutral', demo = false }: StatTileProps) {
  return (
    <div className={`rounded-xl border p-4 shadow-sm ${toneClasses[tone]}`}>
      <div className="flex items-center justify-between gap-2 text-sm font-medium text-slate-500 dark:text-slate-400">
        <span>{label}</span>
        {demo && (
          <span className="rounded bg-violet-100 px-1.5 py-0.5 text-[10px] font-bold tracking-wide text-violet-800 dark:bg-violet-900 dark:text-violet-100">
            DEMO
          </span>
        )}
      </div>
      <div className="mt-1 text-2xl font-semibold text-slate-900 dark:text-slate-50">{value}</div>
      {hint && <div className="mt-1 text-xs text-slate-500 dark:text-slate-400">{hint}</div>}
    </div>
  )
}
