import { useCallback, useEffect, useState } from 'react'

// Hell/Dunkel-Umschaltung. "system" folgt der Einstellung von Handy/Laptop.
// Die Wahl wird im Browser gemerkt (localStorage, nur Komfort - fehlt sie, gilt "system").
// Gleiche Logik steht in public/theme-init.js (laeuft vor dem ersten Zeichnen).

export type ThemePref = 'system' | 'light' | 'dark'

const KEY = 'sg.theme'
const media = () => window.matchMedia('(prefers-color-scheme: dark)')

function readPref(): ThemePref {
  try {
    const v = localStorage.getItem(KEY)
    if (v === 'light' || v === 'dark') return v
  } catch {
    /* privates Fenster o. ae. */
  }
  return 'system'
}

function apply(pref: ThemePref) {
  const dark = pref === 'dark' || (pref === 'system' && media().matches)
  const root = document.documentElement
  root.setAttribute('data-theme', dark ? 'dark' : 'light')
  // Browser-/Statusleiste passend zur Kopfzeile faerben
  document.querySelector('meta[name="theme-color"]')?.setAttribute('content', dark ? '#1a1a19' : '#ffffff')
}

export function useTheme() {
  const [pref, setPrefState] = useState<ThemePref>(readPref)

  useEffect(() => {
    apply(pref)
    if (pref !== 'system') return
    const m = media()
    const onChange = () => apply('system')
    m.addEventListener('change', onChange)
    return () => m.removeEventListener('change', onChange)
  }, [pref])

  const setPref = useCallback((p: ThemePref) => {
    setPrefState(p)
    try {
      if (p === 'system') localStorage.removeItem(KEY)
      else localStorage.setItem(KEY, p)
    } catch {
      /* egal */
    }
  }, [])

  return { pref, setPref }
}
