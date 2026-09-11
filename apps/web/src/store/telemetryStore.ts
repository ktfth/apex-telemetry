import { create } from 'zustand';
import type { LapComparison } from '@apex-telemetry/contracts';
import { DEMO_LAP_COMPARISON } from '../fixtures/demoBahrain2024';
import { fetchLapComparison } from '../lib/apiClient';

interface TelemetryState {
  comparison: LapComparison;
  comparisonSource: 'api' | 'demo';
  comparisonLoading: boolean;
  comparisonRequestId: number;
  refreshComparison: () => Promise<void>;
  // Cursor e sincronização espacial
  hoveredDistanceM: number | null;
  setHoveredDistanceM: (dist: number | null) => void;

  // Seleção de pilotos e voltas
  refDriverNumber: number;
  refLapNumber: number;
  compDriverNumber: number;
  compLapNumber: number;
  setRefSelection: (driver: number, lap: number) => void;
  setCompSelection: (driver: number, lap: number) => void;

  // Insight ativo/em foco (destaca trecho na pista e nos gráficos)
  activeInsightId: string | null;
  setActiveInsightId: (id: string | null) => void;

  // Painéis colapsáveis (para máxima área analítica em telas desktop)
  leftPanelOpen: boolean;
  rightPanelOpen: boolean;
  bottomPanelOpen: boolean;
  toggleLeftPanel: () => void;
  toggleRightPanel: () => void;
  toggleBottomPanel: () => void;

  // Aba ativa do painel inferior
  bottomTab: 'timeline' | 'race_control' | 'stints' | 'laps';
  setBottomTab: (tab: 'timeline' | 'race_control' | 'stints' | 'laps') => void;
}

export const useTelemetryStore = create<TelemetryState>((set) => ({
  comparison: DEMO_LAP_COMPARISON,
  comparisonSource: 'demo',
  comparisonLoading: false,
  comparisonRequestId: 0,
  refreshComparison: async () => {
    let requestId = 0;
    let selection = { session: 9472, refDriver: 1, refLap: 14, compDriver: 16, compLap: 15 };
    set((state) => {
      requestId = state.comparisonRequestId + 1;
      selection = {
        session: 9472,
        refDriver: state.refDriverNumber,
        refLap: state.refLapNumber,
        compDriver: state.compDriverNumber,
        compLap: state.compLapNumber
      };
      return { comparisonLoading: true, comparisonRequestId: requestId };
    });
    const result = await fetchLapComparison(selection.session, selection.refDriver, selection.refLap, selection.compDriver, selection.compLap);
    set((state) => state.comparisonRequestId === requestId ? {
      comparison: result.data,
      comparisonSource: result.isFromApi ? 'api' : 'demo',
      comparisonLoading: false,
      activeInsightId: result.data.insights[0]?.id ?? null
    } : state);
  },
  hoveredDistanceM: null,
  setHoveredDistanceM: (dist) => set({ hoveredDistanceM: dist }),

  refDriverNumber: 1,
  refLapNumber: 14,
  compDriverNumber: 16,
  compLapNumber: 15,
  setRefSelection: (driver, lap) => set({ refDriverNumber: driver, refLapNumber: lap }),
  setCompSelection: (driver, lap) => set({ compDriverNumber: driver, compLapNumber: lap }),

  activeInsightId: 'ins-t4-loss',
  setActiveInsightId: (id) => set({ activeInsightId: id }),

  leftPanelOpen: true,
  rightPanelOpen: true,
  bottomPanelOpen: true,
  toggleLeftPanel: () => set((s) => ({ leftPanelOpen: !s.leftPanelOpen })),
  toggleRightPanel: () => set((s) => ({ rightPanelOpen: !s.rightPanelOpen })),
  toggleBottomPanel: () => set((s) => ({ bottomPanelOpen: !s.bottomPanelOpen })),

  bottomTab: 'race_control',
  setBottomTab: (tab) => set({ bottomTab: tab })
}));
