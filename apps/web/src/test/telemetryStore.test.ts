import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import type { LapComparison } from '@apex-telemetry/contracts';
import { ApiClientError } from '../lib/apiClient';
import { useTelemetryStore } from '../store/telemetryStore';

vi.mock('../lib/apiClient', async () => {
  const actual = await vi.importActual<typeof import('../lib/apiClient')>('../lib/apiClient');
  return {
    ...actual,
    fetchLapComparison: vi.fn(),
    fetchCircuitGeometry: vi.fn()
  };
});

const { fetchLapComparison } = await import('../lib/apiClient');

function comparisonFixture(overrides: Partial<LapComparison> = {}): LapComparison {
  return {
    schema_version: '1.2.0',
    session_key: 9468,
    circuit_key: 63,
    circuit_name: 'Sakhir',
    country_name: 'Bahrain',
    session_name: 'Qualifying',
    data_source: 'openf1-upstream',
    grid_step_m: 5,
    total_distance_m: 5365,
    track_temperature_c: 21.5,
    air_temperature_c: 18.2,
    reference_lap: {
      driver_number: 1,
      driver_code: 'VER',
      team_name: 'Red Bull Racing',
      team_colour: '#3671c6',
      lap_number: 16,
      lap_time_s: 89.179,
      compound: 'SOFT',
      stint_number: 6,
      stint_lap: 2,
      tyre_age_laps: 1,
      coverage_pct: 99.1,
      samples_count: 345,
      sector_1_s: 28.535,
      sector_2_s: 38.269,
      sector_3_s: 22.375
    },
    comparison_lap: {
      driver_number: 16,
      driver_code: 'LEC',
      team_name: 'Ferrari',
      team_colour: '#e8002d',
      lap_number: 15,
      lap_time_s: 89.48,
      compound: 'SOFT',
      stint_number: 6,
      stint_lap: 2,
      tyre_age_laps: 2,
      coverage_pct: 100,
      samples_count: 349,
      sector_1_s: 28.863,
      sector_2_s: 38.344,
      sector_3_s: 22.273
    },
    corners: [],
    speed_traps: [],
    microsectors: [],
    channels: [],
    insights: [
      {
        id: 'seg-525-1095',
        driver_code: 'LEC',
        distance_start_m: 525,
        distance_end_m: 1095,
        time_loss_s: 0.218,
        summary: 'C1: 0.218 s perdidos.',
        evidence: {
          min_speed_ref_kmh: 71.1,
          min_speed_comp_kmh: 66,
          full_throttle_distance_ref_m: 800,
          full_throttle_distance_comp_m: 835,
          braking_point_diff_m: -4.2
        },
        assumptions: [],
        limitations: [],
        confidence: 0.94
      }
    ],
    quality_audit: {
      max_interpolation_gap_m: 39.33,
      discontinuous_segments: 0,
      coverage_pct: 100,
      confidence_score: 0.99,
      median_sample_interval_s: 0.24,
      raw_samples_ref: 345,
      raw_samples_comp: 349,
      delta_closure_error_s: 0,
      source_notes: 'integração trapezoidal'
    },
    ...overrides
  };
}

function resetStore() {
  useTelemetryStore.setState({
    sessionKey: null,
    sessionLabel: '',
    refDriverNumber: null,
    refLapNumber: null,
    compDriverNumber: null,
    compLapNumber: null,
    comparison: { data: null, loading: false, error: null, source: null },
    circuit: { data: null, loading: false, error: null, source: null },
    activeInsightId: null,
    hoveredDistanceM: null,
    liveTick: null,
    liveStreaming: false,
    replayActive: false
  });
}

describe('telemetryStore', () => {
  beforeEach(() => {
    resetStore();
    vi.mocked(fetchLapComparison).mockReset();
  });

  afterEach(() => {
    vi.restoreAllMocks();
  });

  it('parte sem nenhum dado: não existe comparação de demonstração no estado inicial', () => {
    const state = useTelemetryStore.getState();
    expect(state.comparison.data).toBeNull();
    expect(state.circuit.data).toBeNull();
    expect(state.sessionKey).toBeNull();
    expect(state.comparisonQuery()).toBeNull();
  });

  it('só monta a consulta quando a seleção está completa', () => {
    const store = useTelemetryStore.getState();
    store.setSession(9468, 'Bahrain · Qualifying');
    expect(useTelemetryStore.getState().comparisonQuery()).toBeNull();

    useTelemetryStore.getState().setRefSelection(1, 16, 'VER');
    expect(useTelemetryStore.getState().comparisonQuery()).toBeNull();

    useTelemetryStore.getState().setCompSelection(16, 15, 'LEC');
    expect(useTelemetryStore.getState().comparisonQuery()).toEqual({
      sessionKey: 9468,
      refDriver: 1,
      refLap: 16,
      compDriver: 16,
      compLap: 15,
      stepM: 5.0
    });
  });

  it('trocar de sessão descarta pilotos, voltas, comparação e traçado', () => {
    const store = useTelemetryStore.getState();
    store.setSession(9468, 'Bahrain · Qualifying');
    useTelemetryStore.getState().setRefSelection(1, 16, 'VER');
    useTelemetryStore.getState().setCompSelection(16, 15, 'LEC');
    useTelemetryStore.setState({
      comparison: { data: comparisonFixture(), loading: false, error: null, source: 'openf1-upstream' }
    });

    useTelemetryStore.getState().setSession(9472, 'Bahrain · Race');

    const state = useTelemetryStore.getState();
    expect(state.refDriverNumber).toBeNull();
    expect(state.compLapNumber).toBeNull();
    expect(state.comparison.data).toBeNull();
    expect(state.circuit.data).toBeNull();
  });

  it('guarda a comparação real e ativa o primeiro insight', async () => {
    vi.mocked(fetchLapComparison).mockResolvedValue({
      data: comparisonFixture(),
      source: 'openf1-upstream',
      latencyMs: 42
    });

    const store = useTelemetryStore.getState();
    store.setSession(9468, 'Bahrain · Qualifying');
    useTelemetryStore.getState().setRefSelection(1, 16, 'VER');
    useTelemetryStore.getState().setCompSelection(16, 15, 'LEC');
    await useTelemetryStore.getState().refreshComparison();

    const state = useTelemetryStore.getState();
    expect(state.comparison.loading).toBe(false);
    expect(state.comparison.data?.reference_lap.driver_code).toBe('VER');
    expect(state.comparison.source).toBe('openf1-upstream');
    expect(state.activeInsightId).toBe('seg-525-1095');
  });

  it('um erro do gateway deixa o estado vazio e explicado, nunca preenchido', async () => {
    vi.mocked(fetchLapComparison).mockRejectedValue(
      new ApiClientError('Lap 99 não tem tempo registrado', 'LAP_NOT_FOUND', 404)
    );

    const store = useTelemetryStore.getState();
    store.setSession(9468, 'Bahrain · Qualifying');
    useTelemetryStore.getState().setRefSelection(1, 99, 'VER');
    useTelemetryStore.getState().setCompSelection(16, 15, 'LEC');
    await useTelemetryStore.getState().refreshComparison();

    const state = useTelemetryStore.getState();
    expect(state.comparison.data).toBeNull();
    expect(state.comparison.loading).toBe(false);
    expect(state.comparison.error?.code).toBe('LAP_NOT_FOUND');
    expect(state.activeInsightId).toBeNull();
  });

  it('recusa comparar uma volta com ela mesma sem sequer chamar o gateway', async () => {
    const store = useTelemetryStore.getState();
    store.setSession(9468, 'Bahrain · Qualifying');
    useTelemetryStore.getState().setRefSelection(1, 16, 'VER');
    useTelemetryStore.getState().setCompSelection(1, 16, 'VER');
    await useTelemetryStore.getState().refreshComparison();

    expect(fetchLapComparison).not.toHaveBeenCalled();
    expect(useTelemetryStore.getState().comparison.data).toBeNull();
  });

  it('uma resposta antiga não sobrescreve a seleção mais recente', async () => {
    let resolveFirst: (value: unknown) => void = () => {};
    const slow = new Promise((resolve) => {
      resolveFirst = resolve;
    });

    vi.mocked(fetchLapComparison)
      .mockImplementationOnce(() => slow as never)
      .mockResolvedValueOnce({
        data: comparisonFixture({ session_key: 9472 }),
        source: 'postgresql',
        latencyMs: 10
      });

    const store = useTelemetryStore.getState();
    store.setSession(9468, 'Bahrain · Qualifying');
    useTelemetryStore.getState().setRefSelection(1, 16, 'VER');
    useTelemetryStore.getState().setCompSelection(16, 15, 'LEC');

    const first = useTelemetryStore.getState().refreshComparison();
    const second = useTelemetryStore.getState().refreshComparison();
    await second;

    resolveFirst({ data: comparisonFixture({ session_key: 9468 }), source: 'stale', latencyMs: 900 });
    await first;

    const state = useTelemetryStore.getState();
    expect(state.comparison.source).toBe('postgresql');
    expect(state.comparison.data?.session_key).toBe(9472);
  });

  it('o quadro do replay comanda o cursor espacial compartilhado', () => {
    useTelemetryStore.getState().setLiveTick({
      seq: 42,
      distance_m: 1755,
      elapsed_s: 28.5,
      delta_s: 0.12,
      ref_speed_kmh: 242,
      comp_speed_kmh: 243,
      ref_gear: 7,
      comp_gear: 7,
      ref_throttle_pct: 100,
      comp_throttle_pct: 99,
      ref_brake_pct: 0,
      comp_brake_pct: 0,
      ref_drs: true,
      comp_drs: true
    });

    const state = useTelemetryStore.getState();
    expect(state.liveTick?.seq).toBe(42);
    expect(state.hoveredDistanceM).toBe(1755);
  });

  it('o fator de velocidade do replay fica dentro dos limites aceitos pelo gateway', () => {
    useTelemetryStore.getState().setReplaySpeed(999);
    expect(useTelemetryStore.getState().replaySpeed).toBe(50);
    useTelemetryStore.getState().setReplaySpeed(0);
    expect(useTelemetryStore.getState().replaySpeed).toBe(0.5);
  });
});
