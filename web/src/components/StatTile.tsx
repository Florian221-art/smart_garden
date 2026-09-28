interface StatTileProps {
  label: string
  value: string
  hint?: string
  tone?: 'ok' | 'warn' | 'critical' | 'neutral'
}

// Farbe ist immer NUR eine Zusatzinformation - Wert/Text stehen fuer sich
// (WCAG-Anforderung aus plan-webserver.md Abschnitt 6: nie nur Farbe).
const toneClasses: Record<NonNullable<StatTileProps['tone']>, string> = {
  ok: 'border-green-600/40 bg-green-50 dark:bg-green-950/40',
  warn: 'border-amber-600/40 bg-amber-50 dark:bg-amber-950/40',
  critical: 'border-red-600/40 bg-red-50 dark:bg-red-950/40',
  neutral: 'border-slate-300/60 bg-white dark:bg-slate-900',
}

export default function StatTile({ label, value, hint, tone = 'neutral' }: StatTileProps) {
  return (
    <div className={`rounded-xl border p-4 shadow-sm ${toneClasses[tone]}`}>
      <div className="text-sm font-medium text-slate-500 dark:text-slate-400">{label}</div>
      <div className="mt-1 text-2xl font-semibold text-slate-900 dark:text-slate-50">{value}</div>
      {hint && <div className="mt-1 text-xs text-slate-500 dark:text-slate-400">{hint}</div>}
    </div>
  )
}
