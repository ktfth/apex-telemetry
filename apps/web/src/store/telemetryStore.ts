import { create } from 'zustand';

interface TelemetryState {
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
