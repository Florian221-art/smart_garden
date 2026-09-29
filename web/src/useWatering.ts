// "Jetzt giessen" vom Dashboard aus - verfolgt den Befehl vom Klick bis zur Bestaetigung.
//
// Ablauf (api-contract.md Abschnitt 2, commands.pump_run_s):
//   1. POST /commands {pump_run_s}  -> Server merkt sich den Befehl        (phase "queued")
//   2. ESP meldet sich (alle interval_s) und bekommt den Befehl in der
//      Antwort; der Server loescht ihn dabei                              (phase "delivered")
//   3. ESP giesst sofort (max_pump_s_per_run, harte Grenzen im ESP) und
//      schickt mit seiner NAECHSTEN Meldung pump_on_s_since_last > 0     (phase "done")
//      Kommt dort 0, hat eine Sicherheitsgrenze im ESP gegriffen          (phase "notRun")
//      (Tank laut Schaetzung leer, Tageslimit, Mindestpause 10 s).
//
// Der ESP meldet nicht, WARUM er nicht gegossen hat - nur, dass keine Pumpenlaufzeit kam.
import { useCallback, useEffect, useRef, useState } from 'react'
import { sendJson } from './api'
import type { LatestReading, PendingCommands } from './types'

export type WaterPhase = 'idle' | 'sending' | 'queued' | 'delivered' | 'done' | 'notRun' | 'error'

export interface WaterState {
  phase: WaterPhase
  /** Zeitpunkt des Klicks (Browser-Uhr), fuer "wartet schon lange"-Hinweis */
  sentAt: number | null
  /** Tatsaechliche Pumpenlaufzeit laut ESP (nur bei "done") */
  pumpedS: number | null
  error: string | null
}

/** Laengster Pumpenstoss vom Dashboard aus - wie MAX_PUMP_S_PER_RUN im Server (config.py).
 *  Die Pumpe ist stark: mehr als 0,5 s setzt den Kuebel unter Wasser. */
export const MAX_PUMP_S = 0.5

const PENDING_POLL_MS = 2000
const RESULT_POLL_MS = 3000
const DONE_VISIBLE_MS = 45000

const IDLE: WaterState = { phase: 'idle', sentAt: null, pumpedS: null, error: null }

export function useWatering(deviceId: string) {
  const base = `/api/v1/devices/${deviceId}`
  const [state, setState] = useState<WaterState>(IDLE)
  // received_at der Meldung, mit der der ESP den Befehl abgeholt hat
  const deliveredAtRef = useRef<string | null>(null)

  const evaluate = useCallback((r: LatestReading | null) => {
    const deliveredAt = deliveredAtRef.current
    if (!r || !deliveredAt) return
    // Erst die Meldung NACH der Abhol-Meldung sagt, ob gegossen wurde
    if (new Date(r.received_at) <= new Date(deliveredAt)) return
    deliveredAtRef.current = null
    if (r.pump_on_s_since_last > 0 || r.pump_running) {
      setState((s) => ({ ...s, phase: 'done', pumpedS: r.pump_on_s_since_last }))
    } else {
      setState((s) => ({ ...s, phase: 'notRun' }))
    }
  }, [])

  // Beim Laden: wartet noch ein Giess-Befehl (z. B. nach Neuladen der Seite)?
  useEffect(() => {
    let cancelled = false
    fetch(`${base}/commands`)
      .then((res) => (res.ok ? (res.json() as Promise<PendingCommands>) : null))
      .then((p) => {
        if (!cancelled && p && p.pump_run_s > 0) setState({ ...IDLE, phase: 'queued', sentAt: Date.now() })
      })
      .catch(() => {})
    return () => {
      cancelled = true
    }
  }, [base])

  // Phase "queued": warten, bis der ESP den Befehl abgeholt hat
  useEffect(() => {
    if (state.phase !== 'queued') return
    let cancelled = false
    const tick = async () => {
      try {
        const res = await fetch(`${base}/commands`)
        if (!res.ok || cancelled) return
        const p: PendingCommands = await res.json()
        if (p.pump_run_s > 0 || cancelled) return
        // Abgeholt. Die Abhol-Meldung ist im selben Schritt gespeichert worden wie das
        // Loeschen des Befehls -> die jetzt neueste Meldung ist mindestens diese.
        const latest = await fetch(`${base}/latest`)
        if (!latest.ok || cancelled) return
        const r: LatestReading = await latest.json()
        deliveredAtRef.current = r.received_at
        setState((s) => ({ ...s, phase: 'delivered' }))
      } catch {
        /* naechster Versuch */
      }
    }
    const id = setInterval(tick, PENDING_POLL_MS)
    void tick()
    return () => {
      cancelled = true
      clearInterval(id)
    }
  }, [state.phase, base])

  // Phase "delivered": auf die naechste Meldung warten (eigene, schnellere Abfrage)
  useEffect(() => {
    if (state.phase !== 'delivered') return
    let cancelled = false
    const tick = async () => {
      try {
        const res = await fetch(`${base}/latest`)
        if (res.ok && !cancelled) evaluate(await res.json())
      } catch {
        /* naechster Versuch */
      }
    }
    const id = setInterval(tick, RESULT_POLL_MS)
    void tick()
    return () => {
      cancelled = true
      clearInterval(id)
    }
  }, [state.phase, base, evaluate])

  // Erfolg/Hinweis nach einer Weile wieder ausblenden
  useEffect(() => {
    if (state.phase !== 'done' && state.phase !== 'notRun') return
    const id = setTimeout(() => setState(IDLE), DONE_VISIBLE_MS)
    return () => clearTimeout(id)
  }, [state.phase])

  const water = useCallback(
    async (seconds: number) => {
      deliveredAtRef.current = null
      setState({ ...IDLE, phase: 'sending', sentAt: Date.now() })
      try {
        const p = await sendJson<PendingCommands>(`${base}/commands`, 'POST', {
          pump_run_s: Math.min(seconds, MAX_PUMP_S),
        })
        setState((s) => ({ ...s, phase: p.pump_run_s > 0 ? 'queued' : 'error', error: null }))
      } catch (e) {
        setState((s) => ({ ...s, phase: 'error', error: e instanceof Error ? e.message : String(e) }))
      }
    },
    [base],
  )

  const reset = useCallback(() => {
    deliveredAtRef.current = null
    setState(IDLE)
  }, [])

  const busy = state.phase === 'sending' || state.phase === 'queued' || state.phase === 'delivered'
  return { state, water, reset, busy }
}
