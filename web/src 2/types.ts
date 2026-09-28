// Typen fuer GET /api/v1/devices/{id}/latest, exakt nach .claude/api-contract.md v1.2
export interface LatestReading {
  device_id: string
  received_at: string
  fw_version: string
  seq: number
  uptime_s: number
  rssi_dbm: number
  soil_moisture_pct: number | null
  soil_moisture_raw: number | null
  light_pct: number | null
  light_raw: number | null
  air_temp_c: number | null
  air_humidity_pct: number | null
  tank_remaining_ml: number
  water_level_pct: number
  pump_on_s_since_last: number
  pump_running: boolean
  auto_water_triggered: boolean
  errors: string[]
  demo_overrides: string[]
}
