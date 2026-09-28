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

// GET /api/v1/devices/{id}/config
export interface DeviceConfig {
  interval_s: number
  moisture_min_pct: number
  moisture_target_pct: number
  auto_water: boolean
  max_pump_s_per_run: number
  pump_cooldown_s: number
  max_pump_s_per_day: number
  buzzer_enabled: boolean
  tank_capacity_ml: number
  pump_flow_ml_per_s: number
  tank_low_pct: number
}

// Ein Zeit-Bucket aus GET /api/v1/devices/{id}/readings (Mittelwerte bzw. Summen)
export interface HistoryPoint {
  t: string
  n: number
  soil_moisture_pct: number | null
  light_pct: number | null
  air_temp_c: number | null
  air_humidity_pct: number | null
  water_level_pct: number | null
  tank_remaining_ml: number | null
  rssi_dbm: number | null
  pump_on_s: number
  water_ml: number
  auto_water_count: number
  demo: boolean
  errors: string[]
}

export interface HistoryResponse {
  device_id: string
  from: string
  to: string
  bucket_s: number
  config: DeviceConfig
  points: HistoryPoint[]
}

// GET/PUT/DELETE /api/v1/devices/{id}/demo
export interface DemoState {
  active: boolean
  overrides: Partial<Record<OverrideField, number>>
  force_errors: string[]
  scenario: string | null
  started_at: string | null
  expires_at: string | null
  remaining_s: number
}

export type OverrideField =
  | 'soil_moisture_pct'
  | 'light_pct'
  | 'air_temp_c'
  | 'air_humidity_pct'
  | 'water_level_pct'

// GET/POST /api/v1/devices/{id}/commands (noch nicht zugestellte Befehle)
export interface PendingCommands {
  pump_run_s: number
  buzzer: boolean
  identify: boolean
  tank_refilled: boolean
}
