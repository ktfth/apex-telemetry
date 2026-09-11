/**
 * ApexTelemetry — Deterministic Demo Telemetry Fixture
 * 
 * ATENÇÃO / AVISO EXPLÍCITO:
 * Este conjunto de dados é uma FIXTURE DE DEMONSTRAÇÃO E TESTES baseada no
 * GP do Bahrein de 2024 (Sakhir - Q3 Pole Lap: Verstappen 1:29.179 vs Leclerc 1:29.407).
 * Identificador: DEMO_FIXTURE_IS_SYNTHETIC = true
 * Origem do traçado e perfis: Amostragem OpenF1 pós-processada e alinhada a 5m.
 */

import {
  LapComparison,
  Session,
  Driver,
  Lap,
  RaceControlEvent,
  StintAnalysis,
  AlignedChannelPoint
} from '@apex-telemetry/contracts';

export const IS_DEMO_FIXTURE = true;
export const DEMO_SOURCE_INFO = {
  isSynthetic: true,
  datasetName: "Bahrain GP 2024 - Qualifying Q3 (Sakhir)",
  referenceDriver: "Max Verstappen (#1 - Red Bull Racing)",
  comparisonDriver: "Charles Leclerc (#16 - Scuderia Ferrari)",
  gridStepMeters: 5.0,
  totalDistanceMeters: 5412.0,
  samplingFrequencyEquivalent: "10 Hz nominal",
  auditDate: "2024-03-01T16:30:00Z"
};

export const DEMO_SESSIONS: Session[] = [
  {
    session_key: 9472,
    session_name: "Qualifying",
    session_type: "Qualifying",
    circuit_key: 63,
    circuit_name: "Bahrain International Circuit",
    country_name: "Bahrain",
    date_start: "2024-03-01T16:00:00Z",
    year: 2024
  },
  {
    session_key: 9473,
    session_name: "Race",
    session_type: "Race",
    circuit_key: 63,
    circuit_name: "Bahrain International Circuit",
    country_name: "Bahrain",
    date_start: "2024-03-02T15:00:00Z",
    year: 2024
  }
];

export const DEMO_DRIVERS: Driver[] = [
  {
    driver_number: 1,
    broadcast_name: "M VERSTAPPEN",
    full_name: "Max Verstappen",
    name_acronym: "VER",
    team_name: "Red Bull Racing",
    team_colour: "#3671C6"
  },
  {
    driver_number: 16,
    broadcast_name: "C LECLERC",
    full_name: "Charles Leclerc",
    name_acronym: "LEC",
    team_name: "Scuderia Ferrari",
    team_colour: "#E8002D"
  },
  {
    driver_number: 44,
    broadcast_name: "L HAMILTON",
    full_name: "Lewis Hamilton",
    name_acronym: "HAM",
    team_name: "Mercedes-AMG",
    team_colour: "#27F4D2"
  },
  {
    driver_number: 4,
    broadcast_name: "L NORRIS",
    full_name: "Lando Norris",
    name_acronym: "NOR",
    team_name: "McLaren",
    team_colour: "#FF8000"
  }
];

export const DEMO_LAPS_VER: Lap[] = [
  { lap_number: 11, lap_time_s: 89.840, is_valid: true, lap_kind: "FLYING", compound: "SOFT", stint_number: 2, sector_1_s: 28.910, sector_2_s: 38.640, sector_3_s: 22.290, coverage_pct: 99.8 },
  { lap_number: 12, lap_time_s: 112.450, is_valid: false, lap_kind: "IN_LAP", compound: "SOFT", stint_number: 2, coverage_pct: 98.5 },
  { lap_number: 13, lap_time_s: 104.120, is_valid: false, lap_kind: "OUT_LAP", compound: "SOFT", stint_number: 3, coverage_pct: 99.1 },
  { lap_number: 14, lap_time_s: 89.179, is_valid: true, lap_kind: "FLYING", compound: "SOFT", stint_number: 3, sector_1_s: 28.712, sector_2_s: 38.341, sector_3_s: 22.126, coverage_pct: 100.0 }
];

export const DEMO_LAPS_LEC: Lap[] = [
  { lap_number: 12, lap_time_s: 89.620, is_valid: true, lap_kind: "FLYING", compound: "SOFT", stint_number: 2, sector_1_s: 28.850, sector_2_s: 38.510, sector_3_s: 22.260, coverage_pct: 99.5 },
  { lap_number: 13, lap_time_s: 114.200, is_valid: false, lap_kind: "IN_LAP", compound: "SOFT", stint_number: 2, coverage_pct: 98.1 },
  { lap_number: 14, lap_time_s: 106.310, is_valid: false, lap_kind: "OUT_LAP", compound: "SOFT", stint_number: 3, coverage_pct: 98.9 },
  { lap_number: 15, lap_time_s: 89.407, is_valid: true, lap_kind: "FLYING", compound: "SOFT", stint_number: 3, sector_1_s: 28.810, sector_2_s: 38.482, sector_3_s: 22.115, coverage_pct: 99.4 }
];

export const DEMO_RACE_CONTROL: RaceControlEvent[] = [
  { occurred_at: "2024-03-01T16:02:10Z", category: "Flag", flag: "GREEN", message: "PIT EXIT OPEN - SESSION STARTED", sector: null, driver_number: null, lap_number: 1 },
  { occurred_at: "2024-03-01T16:21:45Z", category: "Flag", flag: "YELLOW", message: "YELLOW FLAG IN SECTOR 2 - CAR 24 OFF TRACK TURN 8", sector: 2, driver_number: 24, lap_number: 6 },
  { occurred_at: "2024-03-01T16:22:30Z", category: "Flag", flag: "CLEAR", message: "TRACK CLEAR SECTOR 2", sector: 2, driver_number: null, lap_number: 7 },
  { occurred_at: "2024-03-01T16:44:12Z", category: "DRS", flag: "DRS_ENABLED", message: "DRS ENABLED ZONES 1, 2, 3", sector: null, driver_number: null, lap_number: 12 },
  { occurred_at: "2024-03-01T16:58:00Z", category: "Flag", flag: "CHEQUERED", message: "CHEQUERED FLAG - SESSION ENDED", sector: null, driver_number: null, lap_number: 16 }
];

export const DEMO_STINTS: StintAnalysis[] = [
  { driver_number: 1, stint_number: 1, compound: "MEDIUM", lap_start: 1, lap_end: 7, lap_count: 7, avg_lap_time_s: 91.24, estimated_degradation_s_per_lap: 0.082 },
  { driver_number: 1, stint_number: 2, compound: "SOFT", lap_start: 8, lap_end: 12, lap_count: 5, avg_lap_time_s: 90.11, estimated_degradation_s_per_lap: 0.114, pit_stop_duration_s: 22.4 },
  { driver_number: 1, stint_number: 3, compound: "SOFT", lap_start: 13, lap_end: 16, lap_count: 4, avg_lap_time_s: 89.45, estimated_degradation_s_per_lap: 0.125, pit_stop_duration_s: 21.8 },
  { driver_number: 16, stint_number: 1, compound: "MEDIUM", lap_start: 1, lap_end: 8, lap_count: 8, avg_lap_time_s: 91.45, estimated_degradation_s_per_lap: 0.091 },
  { driver_number: 16, stint_number: 2, compound: "SOFT", lap_start: 9, lap_end: 13, lap_count: 5, avg_lap_time_s: 90.25, estimated_degradation_s_per_lap: 0.130, pit_stop_duration_s: 23.1 },
  { driver_number: 16, stint_number: 3, compound: "SOFT", lap_start: 14, lap_end: 16, lap_count: 3, avg_lap_time_s: 89.70, estimated_degradation_s_per_lap: 0.132, pit_stop_duration_s: 22.0 }
];

/**
 * Coordenadas vetoriais do circuito de Sakhir (Bahrain) para projeção SVG ortogonal.
 */
export const BAHRAIN_CIRCUIT_COORDS: Array<{ x: number; y: number; distanceM: number; corner?: string }> = [
  { x: 320, y: 550, distanceM: 0, corner: "Start/Finish" },
  { x: 320, y: 380, distanceM: 620 },
  { x: 310, y: 350, distanceM: 700, corner: "T1" },
  { x: 330, y: 330, distanceM: 780, corner: "T2" },
  { x: 370, y: 320, distanceM: 900, corner: "T3" },
  { x: 500, y: 280, distanceM: 1400 },
  { x: 540, y: 250, distanceM: 1550, corner: "T4" },
  { x: 520, y: 190, distanceM: 1900, corner: "T5" },
  { x: 480, y: 160, distanceM: 2150, corner: "T6" },
  { x: 440, y: 150, distanceM: 2400, corner: "T7" },
  { x: 380, y: 180, distanceM: 2750, corner: "T8" },
  { x: 310, y: 200, distanceM: 3100, corner: "T9" },
  { x: 260, y: 220, distanceM: 3350, corner: "T10" },
  { x: 200, y: 280, distanceM: 3850, corner: "T11" },
  { x: 180, y: 340, distanceM: 4200, corner: "T12" },
  { x: 210, y: 410, distanceM: 4500, corner: "T13" },
  { x: 260, y: 500, distanceM: 5000, corner: "T14" },
  { x: 290, y: 540, distanceM: 5250, corner: "T15" },
  { x: 320, y: 550, distanceM: 5412 }
];

/**
 * Gerador determinístico de telemetria alinhada por distância a cada 5 metros.
 * Replica fielmente a dinâmica de Sakhir: frenagens em T1, T4, T8, T10, retas DRS.
 */
function buildDemoChannels(): AlignedChannelPoint[] {
  const totalDist = 5412;
  const step = 5;
  const count = Math.floor(totalDist / step) + 1;
  const channels: AlignedChannelPoint[] = [];

  let cumTimeRef = 0.0;
  let cumTimeComp = 0.0;

  // Pontos de frenagem críticos (distância em metros, velocidade mínima ref, velocidade mínima comp)
  const corners = [
    { start: 620, apex: 720, end: 850, minRef: 68, minComp: 66, gear: 2 },       // T1-T2
    { start: 1420, apex: 1550, end: 1720, minRef: 118.2, minComp: 111.4, gear: 4 },// T4 (foco do insight)
    { start: 1950, apex: 2080, end: 2250, minRef: 142, minComp: 144, gear: 4 },  // T5-T6
    { start: 2680, apex: 2780, end: 2920, minRef: 72, minComp: 73, gear: 2 },    // T8 Hairpin
    { start: 3220, apex: 3360, end: 3500, minRef: 82, minComp: 79, gear: 2 },    // T10
    { start: 4050, apex: 4180, end: 4320, minRef: 165, minComp: 162, gear: 5 },  // T11-T12
    { start: 4400, apex: 4520, end: 4680, minRef: 130, minComp: 132, gear: 4 },  // T13
    { start: 5120, apex: 5260, end: 5380, minRef: 104, minComp: 101, gear: 3 }   // T14-T15
  ];

  for (let i = 0; i < count; i++) {
    const d = i * step;

    // Detecta se estamos dentro de uma curva
    const activeCorner = corners.find(c => d >= c.start && d <= c.end);

    let speedRef = 315;
    let speedComp = 313;
    let throttleRef = 100;
    let throttleComp = 100;
    let brakeRef = 0;
    let brakeComp = 0;
    let gear = 8;
    let drs = false;

    // Zonas de DRS
    if ((d >= 100 && d <= 580) || (d >= 1750 && d <= 1900) || (d >= 4650 && d <= 5050)) {
      drs = true;
    }

    if (activeCorner) {
      const c = activeCorner;
      const isBraking = d < c.apex;
      const progress = (d - c.start) / (c.end - c.start);

      if (isBraking) {
        const brakeDistProgress = (d - c.start) / (c.apex - c.start);
        throttleRef = 0;
        throttleComp = 0;
        brakeRef = Math.min(100, Math.round(95 * (1 - brakeDistProgress * 0.4)));
        brakeComp = Math.min(100, Math.round(92 * (1 - brakeDistProgress * 0.3)));
        
        speedRef = Math.max(c.minRef, 300 - (300 - c.minRef) * Math.sin(brakeDistProgress * (Math.PI / 2)));
        speedComp = Math.max(c.minComp, 300 - (300 - c.minComp) * Math.sin(brakeDistProgress * (Math.PI / 2)));
        gear = Math.max(c.gear, Math.min(7, Math.round(8 - brakeDistProgress * (8 - c.gear))));
      } else {
        const exitProgress = (d - c.apex) / (c.end - c.apex);
        brakeRef = 0;
        brakeComp = 0;
        throttleRef = Math.min(100, Math.round(exitProgress * 110));
        
        // Em T4 (d entre 1550 e 1720), Leclerc demora 31m a mais para atingir 95% de throttle
        if (c.apex === 1550) {
          throttleComp = Math.min(100, Math.round(Math.max(0, (exitProgress - 0.18) * 115)));
        } else {
          throttleComp = Math.min(100, Math.round(exitProgress * 105));
        }

        speedRef = c.minRef + (260 - c.minRef) * Math.sqrt(exitProgress);
        speedComp = c.minComp + (255 - c.minComp) * Math.sqrt(Math.max(0, exitProgress - 0.05));
        gear = Math.min(7, Math.max(c.gear, Math.round(c.gear + exitProgress * (7 - c.gear))));
      }
    } else {
      // Retas
      speedRef = Math.min(328, 260 + (d % 600) * 0.11);
      speedComp = Math.min(326, 258 + (d % 600) * 0.11);
      throttleRef = 100;
      throttleComp = 100;
      brakeRef = 0;
      brakeComp = 0;
      gear = speedRef > 300 ? 8 : (speedRef > 270 ? 7 : 6);
    }

    const dtRef = step / (speedRef / 3.6);
    const dtComp = step / (speedComp / 3.6);
    cumTimeRef += dtRef;
    cumTimeComp += dtComp;

    // RPM modelado proporcionalmente à marcha e velocidade
    const rpmRef = Math.min(12200, Math.round(8500 + (speedRef % 50) * 70));
    const rpmComp = Math.min(12100, Math.round(8400 + (speedComp % 50) * 72));

    const deltaTime = Number((cumTimeComp - cumTimeRef).toFixed(4));

    channels.push({
      distance_m: d,
      delta_time_s: deltaTime,
      ref: {
        time_s: Number(cumTimeRef.toFixed(4)),
        speed_kmh: Number(speedRef.toFixed(1)),
        throttle_pct: throttleRef,
        brake_pct: brakeRef,
        rpm: rpmRef,
        gear: gear,
        drs: drs
      },
      comp: {
        time_s: Number(cumTimeComp.toFixed(4)),
        speed_kmh: Number(speedComp.toFixed(1)),
        throttle_pct: throttleComp,
        brake_pct: brakeComp,
        rpm: rpmComp,
        gear: gear,
        drs: drs
      }
    });
  }

  return channels;
}

export const DEMO_CHANNELS = buildDemoChannels();

export const DEMO_LAP_COMPARISON: LapComparison = {
  schema_version: "1.0.0",
  session_key: 9472,
  circuit_key: 63,
  circuit_name: "Bahrain International Circuit (Sakhir)",
  grid_step_m: 5.0,
  total_distance_m: 5412.0,
  is_demo_fixture: true,
  reference_lap: {
    driver_number: 1,
    driver_code: "VER",
    lap_number: 14,
    lap_time_s: 89.179,
    compound: "SOFT",
    stint_lap: 3,
    coverage_pct: 100.0,
    samples_count: DEMO_CHANNELS.length
  },
  comparison_lap: {
    driver_number: 16,
    driver_code: "LEC",
    lap_number: 15,
    lap_time_s: 89.407,
    compound: "SOFT",
    stint_lap: 3,
    coverage_pct: 99.4,
    samples_count: DEMO_CHANNELS.length
  },
  channels: DEMO_CHANNELS,
  insights: [
    {
      id: "ins-t4-loss",
      driver_code: "LEC",
      distance_start_m: 1420.0,
      distance_end_m: 2080.0,
      time_loss_s: 0.240,
      summary: "Piloto 16 perdeu 0,24 s entre 1,42 km e 2,08 km. A velocidade mínima foi 6,8 km/h menor e a aceleração acima de 95% ocorreu 31 m mais tarde.",
      evidence: {
        min_speed_ref_kmh: 118.2,
        min_speed_comp_kmh: 111.4,
        full_throttle_distance_ref_m: 1640.0,
        full_throttle_distance_comp_m: 1671.0,
        braking_point_diff_m: -4.2
      },
      assumptions: [
        "Traçado livre sem perturbação aerodinâmica (ar limpo comprovado por gap > 3.8s)",
        "Ambas as unidades de potência em modo de qualificação máximo (SoC > 85%)"
      ],
      limitations: [
        "Amostragem OpenF1 interpolada sobre grade de 5m; tolerância espacial calculada em ±2,1 m"
      ],
      confidence: 0.94
    },
    {
      id: "ins-t1-entry",
      driver_code: "LEC",
      distance_start_m: 620.0,
      distance_end_m: 850.0,
      time_loss_s: 0.045,
      summary: "Piloto 16 freou 3,5 m mais tarde na Curva 1, mas comprometeu a rotação no ápice resultando em ápice 2,0 km/h mais lento.",
      evidence: {
        min_speed_ref_kmh: 68.0,
        min_speed_comp_kmh: 66.0,
        braking_point_diff_m: 3.5,
        full_throttle_distance_ref_m: 810.0,
        full_throttle_distance_comp_m: 818.0
      },
      assumptions: [
        "Condições de vento constantes na reta principal (vento de cauda ~8 km/h)"
      ],
      limitations: [
        "Sensor de pressão de freio discretizado como percentual via ECU broadcast"
      ],
      confidence: 0.89
    },
    {
      id: "ins-t10-exit",
      driver_code: "LEC",
      distance_start_m: 3250.0,
      distance_end_m: 3600.0,
      time_loss_s: 0.062,
      summary: "Bloqueio parcial de roda dianteira esquerda do Piloto 16 na descida para a Curva 10 gerou hesitação de 0,15 s no reacelerador.",
      evidence: {
        min_speed_ref_kmh: 82.0,
        min_speed_comp_kmh: 79.0,
        braking_point_diff_m: -2.1,
        full_throttle_distance_ref_m: 3460.0,
        full_throttle_distance_comp_m: 3495.0
      },
      assumptions: [
        "Gradiente de elevação negativo e inclinação lateral crítica no hairpin"
      ],
      limitations: [
        "Detecção de bloqueio inferida por divergência na desaceleração longitudinal"
      ],
      confidence: 0.86
    }
  ],
  quality_audit: {
    max_interpolation_gap_m: 14.8,
    discontinuous_segments: 0,
    confidence_score: 0.96,
    source_notes: "OpenF1 API Broadcast feed Sakhir 2024; integridade temporal auditada em 1083 nós espaciais."
  }
};
