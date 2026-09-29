// Zahlen- und Datumsformatierung passend zur gewaehlten Sprache (Komma vs. Punkt,
// 24-Stunden-Format). Wird mit i18n.language aufgerufen (siehe useTranslation()).
import type { Lang } from './i18n'

// en-GB statt en-US: 24-Stunden-Uhrzeit, damit sich Diagramme/Zeiten zwischen den
// Sprachen nur im Wortlaut unterscheiden, nicht im Format (kein AM/PM-Sprung).
const INTL_LOCALE: Record<Lang, string> = { de: 'de-DE', en: 'en-GB', nl: 'nl-NL' }

function resolve(lang: string): string {
  return INTL_LOCALE[lang as Lang] ?? INTL_LOCALE.de
}

export function fmtNumber(lang: string, value: number, digits = 1): string {
  return new Intl.NumberFormat(resolve(lang), { minimumFractionDigits: digits, maximumFractionDigits: digits }).format(
    value,
  )
}

export function fmtInt(lang: string, value: number): string {
  return new Intl.NumberFormat(resolve(lang)).format(Math.round(value))
}

export function fmtDateTime(lang: string, iso: string, opts: Intl.DateTimeFormatOptions): string {
  return new Date(iso).toLocaleString(resolve(lang), opts)
}

export function fmtTimestamp(lang: string, ts: number, opts: Intl.DateTimeFormatOptions): string {
  return new Date(ts).toLocaleString(resolve(lang), opts)
}

export function fmtDate(lang: string, ts: number, opts: Intl.DateTimeFormatOptions): string {
  return new Date(ts).toLocaleDateString(resolve(lang), opts)
}

export function decimalSeparator(lang: string): string {
  return new Intl.NumberFormat(resolve(lang)).format(1.1).includes(',') ? ',' : '.'
}
