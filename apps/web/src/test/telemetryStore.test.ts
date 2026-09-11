import { describe, it, expect, beforeEach } from 'vitest';
import { useTelemetryStore } from '../store/telemetryStore';

describe('telemetryStore and reactive state', () => {
  beforeEach(() => {
    // Reset initial values
    useTelemetryStore.setState({
      refDriverNumber: 1,
      refLapNumber: 14,
      compDriverNumber: 16,
      compLapNumber: 15,
      hoveredDistanceM: null,
      activeInsightId: 'ins-t4-loss',
      liveStreaming: false,
      replayActive: false,
      replaySpeed: 1
    });
  });

  it('updates driver and lap selections correctly', () => {
    const store = useTelemetryStore.getState();
    store.setRefSelection(4, 12);
    store.setCompSelection(44, 13);

    expect(useTelemetryStore.getState().refDriverNumber).toBe(4);
    expect(useTelemetryStore.getState().refLapNumber).toBe(12);
    expect(useTelemetryStore.getState().compDriverNumber).toBe(44);
    expect(useTelemetryStore.getState().compLapNumber).toBe(13);
  });

  it('toggles layout panels cleanly', () => {
    const store = useTelemetryStore.getState();
    const initialLeft = store.leftPanelOpen;
    store.toggleLeftPanel();
    expect(useTelemetryStore.getState().leftPanelOpen).toBe(!initialLeft);

    const initialBottom = store.bottomPanelOpen;
    store.toggleBottomPanel();
    expect(useTelemetryStore.getState().bottomPanelOpen).toBe(!initialBottom);
  });

  it('handles live tick dispatch and updates spatial cursor', () => {
    const store = useTelemetryStore.getState();
    store.setLiveTick({
      seq: 42,
      distance_m: 1550,
      ref_speed_kmh: 310,
      comp_speed_kmh: 305,
      delta_s: 0.15
    });

    expect(useTelemetryStore.getState().liveTick?.seq).toBe(42);
    expect(useTelemetryStore.getState().hoveredDistanceM).toBe(1550);
  });

  it('toggles replay active state and speed multiplier', () => {
    const store = useTelemetryStore.getState();
    expect(store.replayActive).toBe(false);
    store.toggleReplay();
    expect(useTelemetryStore.getState().replayActive).toBe(true);

    store.setReplaySpeed(2.5);
    expect(useTelemetryStore.getState().replaySpeed).toBe(2.5);
  });

  it('fetches and refreshes comparison with fallback auditability', async () => {
    const store = useTelemetryStore.getState();
    await store.refreshComparison();

    const comparison = useTelemetryStore.getState().comparison;
    expect(comparison).toBeDefined();
    expect(comparison.reference_lap.driver_code).toBeDefined();
    expect(comparison.channels.length).toBeGreaterThan(0);
    expect(['api', 'demo']).toContain(useTelemetryStore.getState().comparisonSource);
  });
});
