import { describe, it, expect } from 'vitest';
import {
  integrateDistance,
  resampleToDistanceGrid,
  computeTimeDelta,
  RawTelemetryPoint
} from '../lib/telemetryMath';

describe('telemetryMath algorithms', () => {
  it('correctly integrates distance from constant speed samples', () => {
    // 72 km/h = 20 m/s. Over 5 seconds = 100 meters.
    const samples: RawTelemetryPoint[] = [
      { timeS: 0, speedKmh: 72, throttlePct: 100, brakePct: 0, rpm: 10000, gear: 4, drs: false },
      { timeS: 1, speedKmh: 72, throttlePct: 100, brakePct: 0, rpm: 10000, gear: 4, drs: false },
      { timeS: 2, speedKmh: 72, throttlePct: 100, brakePct: 0, rpm: 10000, gear: 4, drs: false },
      { timeS: 3, speedKmh: 72, throttlePct: 100, brakePct: 0, rpm: 10000, gear: 4, drs: false },
      { timeS: 4, speedKmh: 72, throttlePct: 100, brakePct: 0, rpm: 10000, gear: 4, drs: false },
      { timeS: 5, speedKmh: 72, throttlePct: 100, brakePct: 0, rpm: 10000, gear: 4, drs: false },
    ];

    const integrated = integrateDistance(samples);
    expect(integrated.length).toBe(6);
    expect(integrated[0].distanceM).toBe(0);
    expect(integrated[integrated.length - 1].distanceM).toBeCloseTo(100, 1);
  });

  it('resamples linearly onto a fixed 5-meter grid', () => {
    const samples = integrateDistance([
      { timeS: 0, speedKmh: 36, throttlePct: 50, brakePct: 0, rpm: 5000, gear: 2, drs: false }, // 10 m/s
      { timeS: 2, speedKmh: 36, throttlePct: 50, brakePct: 0, rpm: 5000, gear: 2, drs: false }, // 20m
      { timeS: 4, speedKmh: 36, throttlePct: 50, brakePct: 0, rpm: 5000, gear: 2, drs: false }, // 40m
    ]);

    const grid = resampleToDistanceGrid(samples, 5.0, 25.0);
    expect(grid.length).toBe(9); // 0, 5, 10, 15, 20, 25, 30, 35, 40
    expect(grid[0].distanceM).toBe(0);
    expect(grid[1].distanceM).toBe(5);
    expect(grid[1].timeS).toBeCloseTo(0.5, 2);
    expect(grid[2].distanceM).toBe(10);
    expect(grid[2].timeS).toBeCloseTo(1.0, 2);
    expect(grid[grid.length - 1].distanceM).toBe(40);
  });

  it('safely handles large gaps without extrapolation', () => {
    const raw: RawTelemetryPoint[] = [
      { timeS: 0, speedKmh: 36, throttlePct: 100, brakePct: 0, rpm: 5000, gear: 3, drs: false },
      { timeS: 1, speedKmh: 36, throttlePct: 100, brakePct: 0, rpm: 5000, gear: 3, drs: false }, // 10m
      // Gap of 50m
      { timeS: 6, speedKmh: 36, throttlePct: 100, brakePct: 0, rpm: 5000, gear: 3, drs: false }, // 60m
    ];
    const integrated = integrateDistance(raw);
    const grid = resampleToDistanceGrid(integrated, 5.0, 20.0); // maxGap = 20m

    // In the gap region (> 20m), points must not be marked as validly interpolated
    const midPoint = grid.find(p => p.distanceM === 25);
    expect(midPoint).toBeDefined();
    expect(midPoint?.isInterpolated).toBe(false);
  });

  it('computes time delta correctly between faster reference and slower comparison', () => {
    const refGrid = [
      { distanceM: 0, timeS: 0.0, speedKmh: 200, throttlePct: 100, brakePct: 0, rpm: 10000, gear: 6, drs: false, isInterpolated: false },
      { distanceM: 5, timeS: 0.1, speedKmh: 200, throttlePct: 100, brakePct: 0, rpm: 10000, gear: 6, drs: false, isInterpolated: false },
      { distanceM: 10, timeS: 0.2, speedKmh: 200, throttlePct: 100, brakePct: 0, rpm: 10000, gear: 6, drs: false, isInterpolated: false },
    ];
    const compGrid = [
      { distanceM: 0, timeS: 0.0, speedKmh: 180, throttlePct: 100, brakePct: 0, rpm: 9500, gear: 5, drs: false, isInterpolated: false },
      { distanceM: 5, timeS: 0.12, speedKmh: 180, throttlePct: 100, brakePct: 0, rpm: 9500, gear: 5, drs: false, isInterpolated: false },
      { distanceM: 10, timeS: 0.25, speedKmh: 180, throttlePct: 100, brakePct: 0, rpm: 9500, gear: 5, drs: false, isInterpolated: false },
    ];

    const deltas = computeTimeDelta(refGrid, compGrid);
    expect(deltas[0]).toBe(0.0);
    expect(deltas[1]).toBeCloseTo(0.02, 4); // comp took 0.02s longer
    expect(deltas[2]).toBeCloseTo(0.05, 4); // comp took 0.05s longer
  });
});
