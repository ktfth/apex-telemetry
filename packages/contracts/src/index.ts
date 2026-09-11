/**
 * ApexTelemetry — Core Data Contracts
 * Schema Version: 1.0.0
 */

export type LapKind = 'FLYING' | 'OUT_LAP' | 'IN_LAP' | 'INVALID' | 'SAFETY_CAR';

export type TyreCompound = 'SOFT' | 'MEDIUM' | 'HARD' | 'INTERMEDIATE' | 'WET' | 'UNKNOWN';

export interface Session {
  session_key: number;
  session_name: string;
  session_type: string;
  circuit_key: number;
  circuit_name: string;
  country_name: string;
  date_start: string; // ISO 8601 UTC
  year: number;
}

export interface Driver {
  driver_number: number;
  broadcast_name: string;
  full_name: string;
  name_acronym: string;
  team_name: string;
  team_colour: string; // Hex color code (e.g. "#3671C6")
}

export interface Lap {
  lap_number: number;
  lap_time_s: number;
  is_valid: boolean;
  lap_kind: LapKind;
  compound: TyreCompound;
  stint_number: number;
  sector_1_s?: number;
  sector_2_s?: number;
  sector_3_s?: number;
  coverage_pct: number;
}

export interface ChannelData {
  time_s: number;
  speed_kmh: number;
  throttle_pct: number; // 0 - 100
  brake_pct: number;    // 0 - 100
  rpm: number;
  gear: number;         // 1 - 8
  drs: boolean;
}

export interface AlignedChannelPoint {
  distance_m: number;
  delta_time_s: number; // Positive means ref is faster, or comp is slower
  ref: ChannelData;
  comp: ChannelData;
}

export interface LapSummaryHeader {
  driver_number: number;
  driver_code: string;
  lap_number: number;
  lap_time_s: number;
  compound: TyreCompound;
  stint_lap: number;
  coverage_pct: number;
  samples_count: number;
}

export interface InsightEvidence {
  min_speed_ref_kmh: number;
  min_speed_comp_kmh: number;
  full_throttle_distance_ref_m: number;
  full_throttle_distance_comp_m: number;
  braking_point_diff_m: number;
  [key: string]: number | string;
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
  confidence: number; // 0.0 to 1.0
}

export interface QualityAudit {
  max_interpolation_gap_m: number;
  discontinuous_segments: number;
  confidence_score: number;
  source_notes: string;
}

export interface LapComparison {
  schema_version: '1.0.0';
  session_key: number;
  circuit_key: number;
  circuit_name: string;
  grid_step_m: number;
  total_distance_m: number;
  reference_lap: LapSummaryHeader;
  comparison_lap: LapSummaryHeader;
  channels: AlignedChannelPoint[];
  insights: Insight[];
  quality_audit: QualityAudit;
  is_demo_fixture?: boolean;
}

export interface RaceControlEvent {
  occurred_at: string; // ISO 8601 UTC
  category: string;
  flag: string;
  message: string;
  sector?: number | null;
  driver_number?: number | null;
  lap_number?: number | null;
}

export interface StintAnalysis {
  driver_number: number;
  stint_number: number;
  compound: TyreCompound;
  lap_start: number;
  lap_end: number;
  lap_count: number;
  avg_lap_time_s: number;
  estimated_degradation_s_per_lap: number;
  pit_stop_duration_s?: number;
}
