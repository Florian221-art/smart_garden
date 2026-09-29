import { CircleAlert, CircleCheck, Hourglass, ShowerHead, TriangleAlert, WifiOff } from 'lucide-react'
import type { LucideIcon } from 'lucide-react'
import type { Overall } from '../status'
import { relativeTime } from '../status'
import { Card, StatusLine, toneSoft, type Tone } from './ui'

const heroIcon: Record<Tone, LucideIcon> = {
  ok: CircleCheck,
  warn: TriangleAlert,
  danger: CircleAlert,
  neutral: Hourglass,
}

interface Props {
  overall: Overall
  since: number | null
  offline: boolean
  pumpRunning: boolean
  error: string | null
}

/** Grosse Karte oben: auf einen Blick, ob alles in Ordnung ist - in ganzen Saetzen */
export default function StatusHero({ overall, since, offline, pumpRunning, error }: Props) {
  const Icon = offline ? WifiOff : heroIcon[overall.tone]
  return (
    <Card as="section" aria-labelledby="status-titel" className="p-5 sm:p-6">
      <div className="flex flex-col gap-4 sm:flex-row sm:items-start sm:justify-between">
        <div className="flex min-w-0 items-start gap-4">
          <span className={`grid size-12 shrink-0 place-items-center rounded-2xl ${toneSoft[overall.tone]}`}>
            <Icon aria-hidden="true" className="size-6" />
          </span>
          <div className="min-w-0">
            <h2 id="status-titel" className="text-xl font-semibold tracking-tight text-fg sm:text-2xl">
              {overall.title}
            </h2>
            {overall.issues.length === 0 && overall.tone === 'ok' && (
              <p className="mt-1 text-sm text-fg-2">Erde, Wasser und Temperatur sind im grünen Bereich.</p>
            )}
            {overall.issues.length > 0 && (
              <ul className="mt-2 flex flex-col gap-1.5">
                {overall.issues.map((i) => (
                  <li key={i.text}>
                    <StatusLine tone={i.tone}>{i.text}</StatusLine>
                  </li>
                ))}
              </ul>
            )}
            {pumpRunning && !offline && (
              <p className="mt-2 inline-flex items-center gap-1.5 text-sm font-medium text-accent">
                <ShowerHead aria-hidden="true" className="size-4" />
                Wird gerade gegossen
              </p>
            )}
          </div>
        </div>

        {since !== null && (
          <div className="flex shrink-0 items-center gap-2 self-start rounded-full bg-surface-2 px-3 py-1.5 text-sm text-fg-2">
            <span
              aria-hidden="true"
              className={`size-2 rounded-full ${offline ? 'bg-danger' : 'bg-accent'}`}
            />
            <span>
              {offline ? 'Offline' : 'Online'} · {relativeTime(since)}
            </span>
          </div>
        )}
      </div>

      {error && (
        <p role="alert" className="mt-4 rounded-xl bg-surface-2 px-4 py-3 text-sm text-fg-2">
          {error}
        </p>
      )}
    </Card>
  )
}
