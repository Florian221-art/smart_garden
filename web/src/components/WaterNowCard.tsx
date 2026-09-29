import { useState } from 'react'
import { useTranslation } from 'react-i18next'
import type { LucideIcon } from 'lucide-react'
import {
  CircleAlert,
  CircleCheck,
  Droplets,
  GlassWater,
  Hourglass,
  LoaderCircle,
  RefreshCw,
  ShowerHead,
  TriangleAlert,
  WifiOff,
} from 'lucide-react'
import type { DeviceConfig, LatestReading, PendingCommands } from '../types'
import { sendJson } from '../api'
import { useWatering } from '../useWatering'
import { soilStatus } from '../status'
import { fmtNumber } from '../format'
import { Button, Card, StatusLine, toneSoft, type Tone } from './ui'

/** Unter diesem Fuellstand sperrt der ESP die Pumpe (TANK_EMPTY_PCT in garden_config.h) */
const TANK_EMPTY_PCT = 5

interface Props {
  deviceId: string
  reading: LatestReading | null
  offline: boolean
  config: DeviceConfig | null
  /** aktuelle Zeit (tickt jede Sekunde im Dashboard) */
  now: number
}

/**
 * "Jetzt giessen": grosser, gut sichtbarer Knopf direkt unter der Statuskarte.
 * Zeigt Schritt fuer Schritt, was mit dem Befehl passiert, bis der ESP das Giessen bestaetigt.
 */
export default function WaterNowCard({ deviceId, reading, offline, config, now }: Props) {
  const { t, i18n } = useTranslation()
  const lang = i18n.language
  const { state, water, busy } = useWatering(deviceId)
  const [refill, setRefill] = useState<{ phase: 'idle' | 'sending' | 'sent' | 'error'; at: number }>({
    phase: 'idle',
    at: 0,
  })

  const pumpS = config?.max_pump_s_per_run ?? 0.5
  const ml = config ? Math.round(pumpS * config.pump_flow_ml_per_s) : null
  const intervalS = config?.interval_s ?? 15

  const soilPct = reading?.soil_moisture_pct ?? null
  const soil = reading ? soilStatus(t, soilPct) : null
  const tankEmpty =
    !!reading && (reading.water_level_pct <= TANK_EMPTY_PCT || reading.errors.includes('tank_empty'))
  const soilDry = soil !== null && soilPct !== null && soil.tone !== 'ok' && soilPct < (config?.moisture_min_pct ?? 30)
  const soilWet = soilPct !== null && soilPct > 80

  const blocked: { icon: LucideIcon; text: string } | null = !reading
    ? { icon: Hourglass, text: t('water.blockedNoData') }
    : offline
      ? { icon: WifiOff, text: t('water.blockedOffline') }
      : tankEmpty
        ? { icon: GlassWater, text: t('water.blockedTankEmpty') }
        : null

  // --- Ueberschrift + Erklaertext je nach Lage ---
  let tone: Tone = 'neutral'
  let title = t('water.title')
  let desc: string
  if (blocked) {
    tone = blocked.icon === GlassWater ? 'danger' : 'neutral'
    desc = blocked.text
  } else if (soilPct === null) {
    desc = t('water.soilUnknown')
  } else if (soilDry) {
    tone = soil?.tone === 'danger' ? 'danger' : 'warn'
    title = t('water.needsWaterTitle')
    desc = t('water.soilDry', { pct: fmtNumber(lang, soilPct, 0) })
  } else if (soilWet) {
    desc = t('water.soilWet', { pct: fmtNumber(lang, soilPct, 0) })
  } else {
    tone = 'ok'
    desc = t('water.soilOk', { pct: fmtNumber(lang, soilPct, 0) })
  }

  // --- Fortschritt des Befehls ---
  const waitingLong = state.phase === 'queued' && state.sentAt !== null && now - state.sentAt > (intervalS * 3 + 10) * 1000
  let progress: { tone: Tone; icon: LucideIcon; text: string; spin?: boolean } | null = null
  switch (state.phase) {
    case 'sending':
      progress = { tone: 'neutral', icon: LoaderCircle, spin: true, text: t('water.sending') }
      break
    case 'queued':
      progress = waitingLong
        ? { tone: 'warn', icon: Hourglass, text: t('water.queuedLong') }
        : { tone: 'neutral', icon: LoaderCircle, spin: true, text: t('water.queued', { s: intervalS }) }
      break
    case 'delivered':
      progress = { tone: 'neutral', icon: LoaderCircle, spin: true, text: t('water.delivered') }
      break
    case 'done':
      if (blocked) break
      progress = {
        tone: 'ok',
        icon: CircleCheck,
        text:
          state.pumpedS && config
            ? t('water.doneAmount', {
                s: fmtNumber(lang, state.pumpedS, 1),
                ml: Math.round(state.pumpedS * config.pump_flow_ml_per_s),
              })
            : t('water.done'),
      }
      break
    case 'notRun':
      progress = { tone: 'warn', icon: TriangleAlert, text: t('water.notRun') }
      break
    case 'error':
      progress = { tone: 'danger', icon: CircleAlert, text: t('water.error', { msg: state.error ?? '' }) }
      break
  }

  const ProgressIcon = progress?.icon

  const sendRefill = async () => {
    setRefill({ phase: 'sending', at: Date.now() })
    try {
      await sendJson<PendingCommands>(`/api/v1/devices/${deviceId}/commands`, 'POST', { tank_refilled: true })
      setRefill({ phase: 'sent', at: Date.now() })
    } catch {
      setRefill({ phase: 'error', at: Date.now() })
    }
  }
  // "Gemeldet"-Hinweis gilt, bis der Tank wieder voll gemeldet wird (hoechstens 2 min)
  const refillSent = refill.phase === 'sent' && tankEmpty && now - refill.at < 120000

  const Icon = blocked?.icon ?? (soilDry ? Droplets : ShowerHead)
  const buttonLabel =
    state.phase === 'sending'
      ? t('water.buttonSending')
      : busy
        ? t('water.buttonBusy')
        : state.phase === 'done' && !blocked
          ? t('water.buttonAgain')
          : t('water.button')

  return (
    <Card as="section" aria-labelledby="giessen-titel" className="p-5 sm:p-6">
      <div className="flex flex-col gap-4 sm:flex-row sm:items-center sm:justify-between">
        <div className="flex min-w-0 items-start gap-4">
          <span className={`grid size-12 shrink-0 place-items-center rounded-2xl ${toneSoft[tone]}`}>
            <Icon aria-hidden="true" className="size-6" />
          </span>
          <div className="min-w-0">
            <h2 id="giessen-titel" className="text-lg font-semibold tracking-tight text-fg">
              {title}
            </h2>
            <p className="mt-0.5 text-sm text-fg-2">{desc}</p>
            {!blocked && (
              <p className="mt-1 text-xs text-muted">
                {ml !== null
                  ? t('water.amountHint', { s: fmtNumber(lang, pumpS, 1), ml })
                  : t('water.amountHintNoMl', { s: fmtNumber(lang, pumpS, 1) })}
                {config?.auto_water && soilPct !== null && (
                  <> {t('water.autoHint', { pct: fmtNumber(lang, config.moisture_min_pct, 0) })}</>
                )}
              </p>
            )}
          </div>
        </div>

        <div className="flex shrink-0 flex-col gap-2 sm:items-end">
          <Button
            variant="primary"
            icon={busy ? LoaderCircle : ShowerHead}
            onClick={() => water(pumpS)}
            disabled={!!blocked || busy}
            aria-describedby="giessen-status"
            className={`w-full px-5 text-base sm:w-auto ${busy ? 'motion-safe:[&>svg]:animate-spin' : ''}`}
          >
            {buttonLabel}
          </Button>
          {tankEmpty && !offline && (
            <Button
              icon={RefreshCw}
              onClick={sendRefill}
              disabled={refill.phase === 'sending' || refillSent}
              className="w-full sm:w-auto"
            >
              {t('water.refillButton')}
            </Button>
          )}
        </div>
      </div>

      <div id="giessen-status" role="status" aria-live="polite">
        {progress && ProgressIcon && (
          <div className={`mt-4 flex items-start gap-2 rounded-xl px-4 py-3 text-sm ${toneSoft[progress.tone]}`}>
            <ProgressIcon
              aria-hidden="true"
              className={`mt-0.5 size-4 shrink-0 ${progress.spin ? 'motion-safe:animate-spin' : ''}`}
            />
            <span className="min-w-0">{progress.text}</span>
          </div>
        )}
        {refillSent && (
          <div className="mt-3">
            <StatusLine tone="ok">{t('water.refillSent', { s: intervalS })}</StatusLine>
          </div>
        )}
        {refill.phase === 'error' && tankEmpty && (
          <div className="mt-3">
            <StatusLine tone="danger">{t('water.refillError')}</StatusLine>
          </div>
        )}
      </div>
    </Card>
  )
}
