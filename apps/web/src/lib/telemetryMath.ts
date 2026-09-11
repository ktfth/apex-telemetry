/**
 * ApexTelemetry — Numerical Math & Distance Resampling Algorithms
 * Implements strict distance-based spatial alignment (ADR-002).
 */

export interface RawTelemetryPoint {
  timeS: number;
  speedKmh: number;
  throttlePct: number;
  brakePct: number;
  rpm: number;
  gear: number;
  drs: boolean;
}

export interface DistanceSample extends RawTelemetryPoint {
  distanceM: number;
}

export interface ResampledPoint {
  distanceM: number;
  timeS: number;
  speedKmh: number;
  throttlePct: number;
  brakePct: number;
  rpm: number;
  gear: number;
  drs: boolean;
  isInterpolated: boolean;
}

/**
 * Integra a distância acumulada para uma sequência de amostras temporais ordenadas.
 * distance[i] = distance[i - 1] + speed_mps[i] * delta_time_seconds
 */
export function integrateDistance(samples: RawTelemetryPoint[]): DistanceSample[] {
  if (samples.length === 0) return [];

  const result: DistanceSample[] = [];
  let accumDistance = 0.0;

  result.push({
    ...samples[0],
    distanceM: 0.0
  });

  for (let i = 1; i < samples.length; i++) {
    const prev = samples[i - 1];
    const curr = samples[i];
    const dt = Math.max(0.0, curr.timeS - prev.timeS);
    const speedMps = (curr.speedKmh / 3.6);
    accumDistance += speedMps * dt;

    result.push({
      ...curr,
      distanceM: Number(accumDistance.toFixed(3))
    });
  }

  return result;
}

/**
 * Reamostra uma volta sobre uma grade uniforme de distância (ex: a cada 5 metros).
 * Aplica interpolação linear segura apenas se a lacuna for menor ou igual a maxGapM.
 */
export function resampleToDistanceGrid(
  samples: DistanceSample[],
  gridStepM: number = 5.0,
  maxGapM: number = 25.0
): ResampledPoint[] {
  if (samples.length < 2) return [];

  const totalDistance = samples[samples.length - 1].distanceM;
  const numSteps = Math.floor(totalDistance / gridStepM);
  const grid: ResampledPoint[] = [];

  let sampleIdx = 0;

  for (let step = 0; step <= numSteps; step++) {
    const targetDist = step * gridStepM;

    // Avança ponteiro até englobar o targetDist
    while (
      sampleIdx < samples.length - 1 &&
      samples[sampleIdx + 1].distanceM < targetDist
    ) {
      sampleIdx++;
    }

    if (sampleIdx >= samples.length - 1) {
      const last = samples[samples.length - 1];
      grid.push({
        distanceM: targetDist,
        timeS: last.timeS,
        speedKmh: last.speedKmh,
        throttlePct: last.throttlePct,
        brakePct: last.brakePct,
        rpm: last.rpm,
        gear: last.gear,
        drs: last.drs,
        isInterpolated: false
      });
      break;
    }

    const p0 = samples[sampleIdx];
    const p1 = samples[sampleIdx + 1];
    const distDelta = p1.distanceM - p0.distanceM;

    if (distDelta > maxGapM) {
      // Lacuna excessiva: não inventar dados (marcar com valores de p0 ou sinalização)
      grid.push({
        distanceM: targetDist,
        timeS: p0.timeS,
        speedKmh: p0.speedKmh,
        throttlePct: p0.throttlePct,
        brakePct: p0.brakePct,
        rpm: p0.rpm,
        gear: p0.gear,
        drs: p0.drs,
        isInterpolated: false
      });
      continue;
    }

    const t = distDelta > 0 ? (targetDist - p0.distanceM) / distDelta : 0;
    const clampedT = Math.max(0, Math.min(1, t));

    grid.push({
      distanceM: targetDist,
      timeS: Number((p0.timeS + clampedT * (p1.timeS - p0.timeS)).toFixed(4)),
      speedKmh: Number((p0.speedKmh + clampedT * (p1.speedKmh - p0.speedKmh)).toFixed(1)),
      throttlePct: Number((p0.throttlePct + clampedT * (p1.throttlePct - p0.throttlePct)).toFixed(1)),
      brakePct: Number((p0.brakePct + clampedT * (p1.brakePct - p0.brakePct)).toFixed(1)),
      rpm: Math.round(p0.rpm + clampedT * (p1.rpm - p0.rpm)),
      gear: clampedT < 0.5 ? p0.gear : p1.gear,
      drs: clampedT < 0.5 ? p0.drs : p1.drs,
      isInterpolated: clampedT > 0 && clampedT < 1
    });
  }

  return grid;
}

/**
 * Calcula o Delta de Tempo acumulado: Delta(d) = TimeComp(d) - TimeRef(d).
 * Valor positivo significa que a referência (Ref) está mais rápida / comp está atrás.
 */
export function computeTimeDelta(
  refGrid: ResampledPoint[],
  compGrid: ResampledPoint[]
): number[] {
  const len = Math.min(refGrid.length, compGrid.length);
  const deltas: number[] = new Array(len);

  for (let i = 0; i < len; i++) {
    const delta = compGrid[i].timeS - refGrid[i].timeS;
    deltas[i] = Number(delta.toFixed(4));
  }

  return deltas;
}
