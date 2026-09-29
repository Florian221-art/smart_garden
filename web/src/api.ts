// Kleine Helfer fuer Aufrufe, die etwas veraendern (Demo, Befehle).
// Fehlertexte von FastAPI (422) werden lesbar gemacht.
import i18n from './i18n'

async function readError(res: Response): Promise<string> {
  try {
    const body = await res.json()
    const d = body?.detail
    if (typeof d === 'string') return d
    if (Array.isArray(d) && d.length > 0) return d.map((x: { msg?: string }) => x.msg ?? '').join('; ')
  } catch {
    /* kein JSON */
  }
  return i18n.t('api.httpError', { status: res.status })
}

export async function sendJson<T>(url: string, method: 'PUT' | 'POST' | 'DELETE', body?: unknown): Promise<T> {
  let res: Response
  try {
    res = await fetch(url, {
      method,
      headers: body === undefined ? undefined : { 'Content-Type': 'application/json' },
      body: body === undefined ? undefined : JSON.stringify(body),
    })
  } catch {
    throw new Error(i18n.t('api.unreachable'))
  }
  if (!res.ok) throw new Error(await readError(res))
  return res.json() as Promise<T>
}
