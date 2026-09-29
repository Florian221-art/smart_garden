// Setzt Hell/Dunkel VOR dem ersten Zeichnen (kein weisses Aufblitzen im Dunkelmodus).
// Eigene Datei statt Inline-Skript, damit spaeter eine strenge CSP (ohne 'unsafe-inline') moeglich ist.
// Gleiche Logik wie src/theme.ts.
(function () {
  var pref = 'system'
  try {
    pref = localStorage.getItem('sg.theme') || 'system'
  } catch (e) {}
  var dark = pref === 'dark' || (pref !== 'light' && window.matchMedia('(prefers-color-scheme: dark)').matches)
  document.documentElement.setAttribute('data-theme', dark ? 'dark' : 'light')
})()
