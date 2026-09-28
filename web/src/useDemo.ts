import { useCallback, useEffect, useState } from 'react'
import type { DemoState } from './types'

const POLL_MS = 3000

/** Demo-Zustand vom Server + sekundengenauer Countdown (lokal gerechnet) */
export function useDemo(deviceId: string) {
  const [demo, setDemo] = useState<DemoState | null>(null)
  const [now, setNow] = useState(() => Date.now())

  const refresh = useCallback(async () => {
    try {
      const res = await fetch(`/api/v1/devices/${deviceId}/demo`)
      if (res.ok) setDemo(await res.json())
    } catch {
      /* Server weg - Anzeige behaelt den letzten Stand */
    }
  }, [deviceId])

  useEffect(() => {
    refresh()
    const poll = setInterval(refresh, POLL_MS)
    const tick = setInterval(() => setNow(Date.now()), 1000)
    return () => {
      clearInterval(poll)
      clearInterval(tick)
    }
  }, [refresh])

  const remaining =
    demo?.active && demo.expires_at ? Math.max(0, Math.ceil((new Date(demo.expires_at).getTime() - now) / 1000)) : 0
  const active = !!demo?.active && remaining > 0

  return { demo: active ? demo : null, remaining, setDemo, refresh }
}

export function fmtCountdown(s: number): string {
  return `${Math.floor(s / 60)}:${String(s % 60).padStart(2, '0')}`
}
