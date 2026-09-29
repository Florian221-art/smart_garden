import type { LucideIcon } from 'lucide-react'
import { useTranslation } from 'react-i18next'
import { Signal, SignalHigh, SignalLow, SignalMedium } from 'lucide-react'
import type { LatestReading } from '../types'
import { fmtDuration, relativeTime, signalLabel } from '../status'
import { fmtDateTime } from '../format'
import { Card, SectionTitle } from './ui'

const SIGNAL_ICON = [SignalLow, SignalMedium, SignalHigh, Signal]

function Row({ label, value, sub }: { label: string; value: React.ReactNode; sub?: string }) {
  return (
    <div className="flex items-baseline justify-between gap-4 py-2.5">
      <dt className="text-sm text-fg-2">{label}</dt>
      <dd className="text-right text-sm font-medium text-fg">
        {value}
        {sub && <span className="block text-xs font-normal text-muted">{sub}</span>}
      </dd>
    </div>
  )
}

/** Technische Angaben - fuer Neugierige und zur Fehlersuche, bewusst unten auf der Seite */
export default function DeviceCard({
  icon,
  reading,
  since,
  offline,
}: {
  icon: LucideIcon
  reading: LatestReading
  since: number
  offline: boolean
}) {
  const { t, i18n } = useTranslation()
  const sig = signalLabel(t, reading.rssi_dbm)
  const SigIcon = SIGNAL_ICON[sig.bars]
  const raw = [
    reading.soil_moisture_raw != null && t('device.rawSoil', { v: reading.soil_moisture_raw }),
    reading.light_raw != null && t('device.rawLight', { v: reading.light_raw }),
  ].filter(Boolean)
  return (
    <section aria-labelledby="geraet-titel">
      <SectionTitle id="geraet-titel" icon={icon}>
        {t('device.title')}
      </SectionTitle>
      <Card className="px-5 py-2">
        <dl className="grid grid-cols-1 divide-y divide-line md:grid-cols-2 md:gap-x-10 md:divide-y-0">
          <div className="divide-y divide-line">
            <Row
              label={t('device.lastMessage')}
              value={offline ? t('device.offlinePrefix', { time: relativeTime(t, since) }) : relativeTime(t, since)}
              sub={fmtDateTime(i18n.language, reading.received_at, { dateStyle: 'short', timeStyle: 'medium' })}
            />
            <Row
              label={t('device.wifiSignal')}
              value={
                <span className="inline-flex items-center gap-1.5">
                  <SigIcon aria-hidden="true" className="size-4 text-muted" />
                  {sig.text}
                </span>
              }
              sub={`${reading.rssi_dbm} dBm`}
            />
            <Row label={t('device.runningSince')} value={fmtDuration(t, reading.uptime_s)} />
          </div>
          <div className="divide-y divide-line">
            <Row label={t('device.deviceName')} value={reading.device_id} />
            <Row label={t('device.firmware')} value={reading.fw_version} sub={t('device.messageNo', { n: reading.seq })} />
            <Row label={t('device.rawValues')} value={raw.length ? raw.join(' · ') : t('device.none')} />
          </div>
        </dl>
      </Card>
    </section>
  )
}
