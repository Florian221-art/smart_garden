// Mehrsprachigkeit (DE/EN/NL) - laut plan-webserver.md Abschnitt "Festgelegter Tech-Stack".
// Alle Texte sind fest im Dashboard eingebaut (kein Nachladen ueber das Netz), damit die
// App auch offline im Hotspot-WLAN vollstaendig funktioniert.
import i18n from 'i18next'
import { initReactI18next } from 'react-i18next'
import LanguageDetector from 'i18next-browser-languagedetector'
import de from './locales/de.json'
import en from './locales/en.json'
import nl from './locales/nl.json'

export const SUPPORTED_LANGUAGES = ['de', 'en', 'nl'] as const
export type Lang = (typeof SUPPORTED_LANGUAGES)[number]
export const LANG_STORAGE_KEY = 'sg.lang'

void i18n
  .use(LanguageDetector)
  .use(initReactI18next)
  .init({
    resources: { de: { translation: de }, en: { translation: en }, nl: { translation: nl } },
    fallbackLng: 'de',
    supportedLngs: SUPPORTED_LANGUAGES,
    load: 'languageOnly',
    interpolation: { escapeValue: false },
    detection: {
      order: ['localStorage', 'navigator'],
      lookupLocalStorage: LANG_STORAGE_KEY,
      caches: ['localStorage'],
      // "en-US" / "nl-BE" -> "en" / "nl", damit Umschalter und Zahlenformat die Sprache erkennen
      convertDetectedLanguage: (lng: string) => lng.split('-')[0],
    },
    // Kein XSS-Risiko durch Nutzereingaben in Uebersetzungen - Interpolation bleibt lesbar.
    returnEmptyString: false,
  })

// <html lang> mitziehen, damit Screenreader die Texte in der richtigen Sprache vorlesen
const syncHtmlLang = (lng: string) => {
  document.documentElement.lang = lng.split('-')[0]
}
syncHtmlLang(i18n.resolvedLanguage ?? i18n.language ?? 'de')
i18n.on('languageChanged', syncHtmlLang)

export default i18n
