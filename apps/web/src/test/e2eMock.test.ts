import { describe, it, expect } from 'vitest';
import { useTelemetryStore } from '../store/telemetryStore';
import { DEMO_DRIVERS, DEMO_SESSIONS } from '../fixtures/demoBahrain2024';

describe('E2E Dashboard User Journey & Integrity Suite', () => {
  it('allows end-to-end user navigation across sessions and drivers', () => {
    const store = useTelemetryStore.getState();

    // 1. User selects Bahrain Qualifying 2024
    expect(DEMO_SESSIONS[0].session_key).toBe(9472);

    // 2. User selects VER (1) as Reference and LEC (16) as Comparison
    store.setRefSelection(1, 14);
    store.setCompSelection(16, 15);

    expect(useTelemetryStore.getState().refDriverNumber).toBe(1);
    expect(useTelemetryStore.getState().compDriverNumber).toBe(16);

    // 3. User clicks on an insight in Turn 4
    store.setActiveInsightId('ins-t4-loss');
    store.setHoveredDistanceM(1550);

    expect(useTelemetryStore.getState().activeInsightId).toBe('ins-t4-loss');
    expect(useTelemetryStore.getState().hoveredDistanceM).toBe(1550);

    // 4. User starts Replay mode
    store.toggleReplay();
    expect(useTelemetryStore.getState().replayActive).toBe(true);

    // 5. User adjusts Replay speed to 2x
    store.setReplaySpeed(2);
    expect(useTelemetryStore.getState().replaySpeed).toBe(2);

    // 6. User pauses Replay
    store.toggleReplay();
    expect(useTelemetryStore.getState().replayActive).toBe(false);
  });

  it('verifies all drivers have valid metadata and colours', () => {
    DEMO_DRIVERS.forEach((driver) => {
      expect(driver.driver_number).toBeGreaterThan(0);
      expect(driver.broadcast_name).toBeDefined();
      expect(driver.team_colour).toMatch(/^#[0-9A-Fa-f]{6}$/);
    });
  });
});
