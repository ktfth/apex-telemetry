import { create } from 'zustand';
import type {
  CircuitGeometry,
  LapComparison,
  LiveTelemetryTick
} from '@apex-telemetry/contracts';
import {
  ApiClientError,
  fetchCircuitGeometry,
  fetchLapComparison,
  type ComparisonQuery
} from '../lib/apiClient';

export type { LiveTelemetryTick };

/**
 * Estado de um recurso que vem do gateway.
 *
 * `data` é `null` até existir um dado real. Não há valor inicial de demonstração:
 * a interface distingue explicitamente "carregando", "sem dados" e "erro" em vez
 * de exibir um gráfico plausível enquanto não sabe de nada.
 */
export interface RemoteResource<T> {
  data: T | null;
  loading: boolean;
  error: ApiClientError | null;
  source: string | null;
}

function idle<T>(): RemoteResource<T> {
  return { data: null, loading: false, error: null, source: null };
}

export type BottomTab = 'race_control' | 'stints' | 'laps';
export type InsightTab = 'insights' | 'speed_traps' | 'microsectors';

export interface TelemetryState {
  // Seleção ativa
  sessionYear: number;
  sessionKey: number | null;
  sessionLabel: string;
  refDriverNumber: number | null;
  refDriverCode: string;
  refLapNumber: number | null;
  compDriverNumber: number | null;
  compDriverCode: string;
  compLapNumber: number | null;

  setSessionYear: (year: number) => void;
  setSession: (sessionKey: number, label: string) => void;
  setRefSelection: (driver: number, lap: number | null, code?: string) => void;
  setCompSelection: (driver: number, lap: number | null, code?: string) => void;

  /** Consulta completa, ou `null` enquanto a seleção estiver incompleta. */
  comparisonQuery: () => ComparisonQuery | null;

  // Recursos remotos
  comparison: RemoteResource<LapComparison>;
  circuit: RemoteResource<CircuitGeometry>;
  refreshComparison: () => Promise<void>;
  refreshCircuit: () => Promise<void>;

  // Cursor espacial compartilhado entre gráficos e mapa
  hoveredDistanceM: number | null;
  setHoveredDistanceM: (distance: number | null) => void;

  activeInsightId: string | null;
  setActiveInsightId: (id: string | null) => void;

  // Layout
  leftPanelOpen: boolean;
  rightPanelOpen: boolean;
  bottomPanelOpen: boolean;
  toggleLeftPanel: () => void;
  toggleRightPanel: () => void;
  toggleBottomPanel: () => void;

  bottomTab: BottomTab;
  setBottomTab: (tab: BottomTab) => void;
  insightTab: InsightTab;
  setInsightTab: (tab: InsightTab) => void;

  // Replay em tempo de pista via SSE
  liveStreaming: boolean;
  liveError: string | null;
  liveTick: LiveTelemetryTick | null;
  replayActive: boolean;
  replaySpeed: number;
  setLiveStreaming: (active: boolean) => void;
  setLiveError: (message: string | null) => void;
  setLiveTick: (tick: LiveTelemetryTick | null) => void;
  toggleReplay: () => void;
  setReplaySpeed: (speed: number) => void;
}

/** Controladores em módulo: não pertencem ao estado renderizável. */
let comparisonController: AbortController | null = null;
let circuitController: AbortController | null = null;

export const useTelemetryStore = create<TelemetryState>((set, get) => ({
  // Ano corrente: o seletor deriva a mesma lista, então padrão e opções coincidem.
  sessionYear: new Date().getUTCFullYear(),
  sessionKey: null,
  sessionLabel: '',
  refDriverNumber: null,
  refDriverCode: '',
  refLapNumber: null,
  compDriverNumber: null,
  compDriverCode: '',
  compLapNumber: null,

  setSessionYear: (year) =>
    set({
      sessionYear: year,
      sessionKey: null,
      sessionLabel: '',
      refDriverNumber: null,
      refLapNumber: null,
      compDriverNumber: null,
      compLapNumber: null,
      comparison: idle(),
      circuit: idle()
    }),

  setSession: (sessionKey, label) => {
    if (get().sessionKey === sessionKey) return;
    // Trocar de sessão invalida pilotos, voltas e traçado: nada sobrevive à troca.
    set({
      sessionKey,
      sessionLabel: label,
      refDriverNumber: null,
      refLapNumber: null,
      compDriverNumber: null,
      compLapNumber: null,
      comparison: idle(),
      circuit: idle(),
      hoveredDistanceM: null,
      activeInsightId: null
    });
  },

  setRefSelection: (driver, lap, code) =>
    set((state) => ({
      refDriverNumber: driver,
      refLapNumber: lap,
      refDriverCode: code ?? (driver === state.refDriverNumber ? state.refDriverCode : '')
    })),

  setCompSelection: (driver, lap, code) =>
    set((state) => ({
      compDriverNumber: driver,
      compLapNumber: lap,
      compDriverCode: code ?? (driver === state.compDriverNumber ? state.compDriverCode : '')
    })),

  comparisonQuery: () => {
    const { sessionKey, refDriverNumber, refLapNumber, compDriverNumber, compLapNumber } = get();
    if (
      sessionKey === null ||
      refDriverNumber === null ||
      refLapNumber === null ||
      compDriverNumber === null ||
      compLapNumber === null
    ) {
      return null;
    }
    return {
      sessionKey,
      refDriver: refDriverNumber,
      refLap: refLapNumber,
      compDriver: compDriverNumber,
      compLap: compLapNumber,
      stepM: 5.0
    };
  },

  comparison: idle(),
  circuit: idle(),

  refreshComparison: async () => {
    const query = get().comparisonQuery();
    if (!query) {
      set({ comparison: idle() });
      return;
    }
    // Comparar uma volta com ela mesma não é uma comparação; o gateway recusaria.
    if (query.refDriver === query.compDriver && query.refLap === query.compLap) {
      set({ comparison: idle() });
      return;
    }

    comparisonController?.abort();
    const controller = new AbortController();
    comparisonController = controller;

    set((state) => ({ comparison: { ...state.comparison, loading: true, error: null } }));

    try {
      const result = await fetchLapComparison(query, controller.signal);
      if (comparisonController !== controller) return;
      set({
        comparison: { data: result.data, loading: false, error: null, source: result.source },
        activeInsightId: result.data.insights[0]?.id ?? null
      });
    } catch (error) {
      if (controller.signal.aborted || comparisonController !== controller) return;
      set({
        comparison: {
          data: null,
          loading: false,
          error:
            error instanceof ApiClientError
              ? error
              : new ApiClientError(String(error), 'UNEXPECTED_ERROR', 0),
          source: null
        },
        activeInsightId: null
      });
    } finally {
      if (comparisonController === controller) comparisonController = null;
    }
  },

  refreshCircuit: async () => {
    const { sessionKey } = get();
    if (sessionKey === null) {
      set({ circuit: idle() });
      return;
    }

    circuitController?.abort();
    const controller = new AbortController();
    circuitController = controller;

    set((state) => ({ circuit: { ...state.circuit, loading: true, error: null } }));

    try {
      const result = await fetchCircuitGeometry(sessionKey, controller.signal);
      if (circuitController !== controller) return;
      set({ circuit: { data: result.data, loading: false, error: null, source: result.source } });
    } catch (error) {
      if (controller.signal.aborted || circuitController !== controller) return;
      set({
        circuit: {
          data: null,
          loading: false,
          error:
            error instanceof ApiClientError
              ? error
              : new ApiClientError(String(error), 'UNEXPECTED_ERROR', 0),
          source: null
        }
      });
    } finally {
      if (circuitController === controller) circuitController = null;
    }
  },

  hoveredDistanceM: null,
  setHoveredDistanceM: (distance) => set({ hoveredDistanceM: distance }),

  activeInsightId: null,
  setActiveInsightId: (id) => set({ activeInsightId: id }),

  leftPanelOpen: true,
  rightPanelOpen: true,
  bottomPanelOpen: true,
  toggleLeftPanel: () => set((state) => ({ leftPanelOpen: !state.leftPanelOpen })),
  toggleRightPanel: () => set((state) => ({ rightPanelOpen: !state.rightPanelOpen })),
  toggleBottomPanel: () => set((state) => ({ bottomPanelOpen: !state.bottomPanelOpen })),

  bottomTab: 'race_control',
  setBottomTab: (tab) => set({ bottomTab: tab }),
  insightTab: 'insights',
  setInsightTab: (tab) => set({ insightTab: tab }),

  liveStreaming: false,
  liveError: null,
  liveTick: null,
  replayActive: false,
  replaySpeed: 4,

  setLiveStreaming: (active) => set({ liveStreaming: active }),
  setLiveError: (message) => set({ liveError: message }),
  setLiveTick: (tick) =>
    set((state) => ({
      liveTick: tick,
      // O quadro do replay comanda o cursor espacial compartilhado.
      hoveredDistanceM: tick ? tick.distance_m : state.hoveredDistanceM
    })),
  toggleReplay: () =>
    set((state) => ({
      replayActive: !state.replayActive,
      liveTick: state.replayActive ? null : state.liveTick,
      liveError: null
    })),
  setReplaySpeed: (speed) => set({ replaySpeed: Math.min(50, Math.max(0.5, speed)) })
}));
