/**
 * ApexTelemetry — contratos de dados públicos.
 *
 * Estes tipos descrevem exatamente o que o `api-gateway-cpp` devolve. Todo campo
 * aqui tem origem em uma medição da OpenF1 ou em um cálculo do motor de
 * alinhamento espacial; nenhum é preenchido com valor de demonstração.
 *
 * Versão do esquema: 1.2.0
 */

export const SCHEMA_VERSION = '1.2.0' as const;

export type LapKind = 'FLYING' | 'OUT_LAP' | 'IN_LAP' | 'INVALID' | 'SAFETY_CAR';

export type TyreCompound = 'SOFT' | 'MEDIUM' | 'HARD' | 'INTERMEDIATE' | 'WET' | 'UNKNOWN';

/** Origem verificável de um payload servido pelo gateway. */
export type DataSource = 'postgresql' | 'openf1-upstream' | string;

/** Causa dominante de uma perda de tempo, determinada pelo motor numérico. */
export type LossCause =
  | 'APEX_SPEED'
  | 'THROTTLE_APPLICATION'
  | 'BRAKING_POINT'
  | 'TOP_SPEED'
  | 'MIXED';

export interface Session {
  session_key: number;
  session_name: string;
  session_type: string;
  circuit_key: number;
  circuit_name: string;
  country_name: string;
  /** ISO 8601 UTC. */
  date_start: string;
  year: number;
}

export interface Driver {
  driver_number: number;
  broadcast_name: string;
  full_name: string;
  name_acronym: string;
  team_name: string;
  /** Cor oficial da equipe em hexadecimal, ex.: "#3671c6". */
  team_colour: string;
}

export interface Lap {
  driver_number: number;
  lap_number: number;
  /** `null` quando a volta não foi cronometrada (saída de box, bandeira, abandono). */
  lap_time_s: number | null;
  is_valid: boolean;
  lap_kind: LapKind;
  compound: TyreCompound;
  stint_number: number;
  tyre_age_laps: number;
  sector_1_s: number | null;
  sector_2_s: number | null;
  sector_3_s: number | null;
  /** Velocidades dos sensores oficiais de passagem, quando registradas. */
  i1_speed_kmh: number | null;
  i2_speed_kmh: number | null;
  st_speed_kmh: number | null;
  /** `null` enquanto a telemetria da volta não tiver sido medida. */
  coverage_pct: number | null;
}

export interface Stint {
  driver_number: number;
  stint_number: number;
  lap_start: number;
  lap_end: number;
  compound: TyreCompound;
  tyre_age_at_start: number;
}

export interface ChannelData {
  time_s: number;
  speed_kmh: number;
  /** 0 a 100. */
  throttle_pct: number;
  /** 0 a 100. */
  brake_pct: number;
  rpm: number;
  gear: number;
  drs: boolean;
}

export interface AlignedChannelPoint {
  distance_m: number;
  /** tempo_comp − tempo_ref: positivo significa a volta de comparação mais lenta. */
  delta_time_s: number;
  ref: ChannelData;
  comp: ChannelData;
}

export interface LapSummaryHeader {
  driver_number: number;
  driver_code: string;
  team_name: string;
  team_colour: string;
  lap_number: number;
  lap_time_s: number;
  compound: TyreCompound;
  stint_number: number;
  stint_lap: number;
  tyre_age_laps: number;
  coverage_pct: number;
  samples_count: number;
  sector_1_s: number | null;
  sector_2_s: number | null;
  sector_3_s: number | null;
}

/**
 * Zona lenta detectada no canal de velocidade da volta de referência.
 * `C1..Cn` é a ordem de aparição na volta, e não a numeração oficial do circuito:
 * a amostragem da OpenF1 não resolve curvas tomadas em carga plena.
 */
export interface Corner {
  index: number;
  label: string;
  entry_distance_m: number;
  apex_distance_m: number;
  exit_distance_m: number;
  apex_speed_kmh: number;
}

export interface SpeedTrap {
  name: string;
  distance_m: number;
  ref_speed_kmh: number;
  comp_speed_kmh: number;
  delta_kmh: number;
  /** `openf1_marshalling_loop` para sensores oficiais; `channel_peak` para picos medidos. */
  origin: 'openf1_marshalling_loop' | 'channel_peak' | string;
}

export interface Microsector {
  index: number;
  distance_start_m: number;
  distance_end_m: number;
  delta_s: number;
  ref_avg_speed_kmh: number;
  comp_avg_speed_kmh: number;
  winner: 'REF' | 'COMP' | 'EQUAL';
}

export interface InsightEvidence {
  min_speed_ref_kmh: number;
  min_speed_comp_kmh: number;
  max_speed_ref_kmh?: number;
  max_speed_comp_kmh?: number;
  full_throttle_distance_ref_m: number;
  full_throttle_distance_comp_m: number;
  braking_point_ref_m?: number;
  braking_point_comp_m?: number;
  braking_point_diff_m: number;
  apex_distance_m?: number;
  is_corner?: boolean;
  cause?: LossCause;
  corner?: string;
}

export interface Insight {
  id: string;
  driver_code: string;
  distance_start_m: number;
  distance_end_m: number;
  time_loss_s: number;
  summary: string;
  evidence: InsightEvidence;
  assumptions: string[];
  limitations: string[];
  /** `strategy-hs` quando o motor Haskell respondeu; `analytics-cpp` no resumo local. */
  engine?: string;
  /** 0.0 a 1.0. */
  confidence: number;
}

export interface QualityAudit {
  max_interpolation_gap_m: number;
  discontinuous_segments: number;
  coverage_pct: number;
  confidence_score: number;
  median_sample_interval_s: number;
  raw_samples_ref: number;
  raw_samples_comp: number;
  /**
   * |delta acumulado no fim da volta − diferença cronometrada oficial|.
   * Verificação independente do alinhamento espacial: quanto maior, menos
   * confiável é a régua de distância.
   */
  delta_closure_error_s: number;
  source_notes: string;
}

export interface LapComparison {
  schema_version: string;
  session_key: number;
  circuit_key: number;
  circuit_name: string;
  country_name: string;
  session_name: string;
  data_source: DataSource;
  grid_step_m: number;
  total_distance_m: number;
  track_temperature_c: number | null;
  air_temperature_c: number | null;
  reference_lap: LapSummaryHeader;
  comparison_lap: LapSummaryHeader;
  corners: Corner[];
  speed_traps: SpeedTrap[];
  microsectors: Microsector[];
  channels: AlignedChannelPoint[];
  insights: Insight[];
  quality_audit: QualityAudit;
}

export interface RaceControlEvent {
  /** ISO 8601 UTC. */
  occurred_at: string;
  category: string;
  flag: string;
  scope: string;
  message: string;
  sector: number | null;
  driver_number: number | null;
  lap_number: number | null;
}

export interface WeatherSample {
  occurred_at: string;
  air_temperature_c: number;
  track_temperature_c: number;
  humidity_pct: number;
  pressure_mbar: number;
  wind_speed_ms: number;
  wind_direction_deg: number;
  rainfall: number;
}

/** Traçado real do circuito, reconstruído do transponder de posição. */
export interface CircuitGeometry {
  session_key: number;
  circuit_key: number;
  circuit_name: string;
  country_name: string;
  reference_driver_number: number;
  reference_lap_number: number;
  reference_lap_time_s: number;
  total_distance_m: number;
  data_source: DataSource;
  /** Cada ponto é `[distância_m, x, y]` nas unidades do transponder da OpenF1. */
  path: Array<[number, number, number]>;
  corners: Array<Corner & { x: number; y: number }>;
}

export interface StrategyRecommendation {
  target_lap: number;
  next_compound: TyreCompound;
  projected_gain_s: number;
  rationale: string;
  confidence: number;
}

/** Análise de um stint real produzida pelo motor de domínio em Haskell. */
export interface StintAnalysis {
  driver_number: number;
  stint_number: number;
  compound: TyreCompound;
  lap_start: number;
  lap_end: number;
  lap_count: number;
  /** Voltas que sobreviveram ao filtro de outliers e alimentaram a regressão. */
  representative_laps: number;
  avg_lap_time_s: number;
  best_lap_time_s: number;
  /** Inclinação medida na regressão dos tempos reais do stint. */
  observed_degradation_s_per_lap: number;
  /** Perda prevista pelo modelo térmico, para contraste com a medida. */
  predicted_pace_loss_s: number;
  cliff_lap: number;
  in_cliff: boolean;
  recommendation: StrategyRecommendation | null;
  notes: string[];
}

export interface DegradationReport {
  session_key: number;
  engine: string;
  track_temperature_c?: number;
  stints: StintAnalysis[];
}

/** Quadro do replay em tempo de pista transmitido por SSE. */
export interface LiveTelemetryTick {
  seq: number;
  distance_m: number;
  elapsed_s: number;
  delta_s: number;
  ref_speed_kmh: number;
  comp_speed_kmh: number;
  ref_gear: number;
  comp_gear: number;
  ref_throttle_pct: number;
  comp_throttle_pct: number;
  ref_brake_pct: number;
  comp_brake_pct: number;
  ref_drs: boolean;
  comp_drs: boolean;
}

/** Erro estruturado do gateway: a ausência de dados é reportada, nunca preenchida. */
export interface ApiError {
  error: string;
  code: string;
}
