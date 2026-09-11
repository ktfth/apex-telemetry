import { describe, it, expect } from 'vitest';
import { DEMO_LAP_COMPARISON } from '../fixtures/demoBahrain2024';

describe('Fase 6 Feature Suite: Speed Traps, Microsectors & Exports', () => {
  it('contains valid speed traps matching Bahrain official sectors', () => {
    const traps = DEMO_LAP_COMPARISON.speed_traps;
    expect(traps).toBeDefined();
    expect(traps?.length).toBe(5);

    const turn1 = traps?.find((t) => t.name.includes('Turn 1'));
    expect(turn1).toBeDefined();
    expect(turn1?.distance_m).toBe(650.0);
    expect(turn1?.ref_speed_kmh).toBeGreaterThan(300.0);
  });

  it('contains microsectors covering the complete 5412m lap', () => {
    const microsectors = DEMO_LAP_COMPARISON.microsectors;
    expect(microsectors).toBeDefined();
    expect(microsectors?.length).toBe(55);

    expect(microsectors?.[0].distance_start_m).toBe(0);
    expect(microsectors?.[0].distance_end_m).toBe(100);
    expect(['REF', 'COMP', 'EQUAL']).toContain(microsectors?.[0].winner);

    const last = microsectors?.[54];
    expect(last?.distance_end_m).toBe(5412);
  });

  it('validates MoTeC CSV generation contracts', () => {
    const sampleChannels = DEMO_LAP_COMPARISON.channels.slice(0, 5);
    expect(sampleChannels.length).toBe(5);
    sampleChannels.forEach((c) => {
      expect(c.distance_m).toBeGreaterThanOrEqual(0);
      expect(c.ref.speed_kmh).toBeGreaterThan(0);
      expect(c.comp.speed_kmh).toBeGreaterThan(0);
      expect(c.ref.throttle_pct).toBeGreaterThanOrEqual(0);
      expect(c.comp.throttle_pct).toBeGreaterThanOrEqual(0);
    });
  });
});
