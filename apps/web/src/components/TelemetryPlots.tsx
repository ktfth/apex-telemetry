'use client';

import React, { useCallback, useMemo, useRef } from 'react';
import { AlertTriangle, Loader2 } from 'lucide-react';
import { MetricInstrument } from '@apex-telemetry/ui';
import type { AlignedChannelPoint, Corner, Insight } from '@apex-telemetry/contracts';
import { useTelemetryStore } from '../store/telemetryStore';

const CHART_WIDTH = 1000;
const SPEED_HEIGHT = 120;
const DELTA_HEIGHT = 70;
const PEDALS_HEIGHT = 80;

/** Extremos com folga, arredondados para um passo legível no eixo. */
function niceBounds(min: number, max: number, step: number): [number, number] {
  const lower = Math.floor(min / step) * step;
  const upper = Math.ceil(max / step) * step;
  return [lower, upper === lower ? lower + step : upper];
}

function buildPath(
  channels: AlignedChannelPoint[],
  totalDistance: number,
  value: (point: AlignedChannelPoint) => number,
  toY: (value: number) => number
): string {
  return channels
    .map((point, index) => {
      const x = (point.distance_m / totalDistance) * CHART_WIDTH;
      return `${index === 0 ? 'M' : 'L'} ${x.toFixed(1)},${toY(value(point)).toFixed(1)}`;
    })
    .join(' ');
}

interface OverlayProps {
  height: number;
  keyPrefix: string;
  activeInsight: Insight | null | undefined;
  insightStartX: number;
  insightWidth: number;
  distanceTicks: number[];
  corners: readonly Corner[];
  totalDistance: number;
}

const Overlay: React.FC<OverlayProps> = React.memo(({
  height,
  keyPrefix,
  activeInsight,
  insightStartX,
  insightWidth,
  distanceTicks,
  corners,
  totalDistance
}) => (
  <>
    {activeInsight && (
      <rect x={insightStartX} y={0} width={insightWidth} height={height} fill="#eab308" fillOpacity="0.12" />
    )}
    {distanceTicks.map((distance) => {
      const x = (distance / totalDistance) * CHART_WIDTH;
      return (
        <line
          key={`${keyPrefix}-${distance}`}
          x1={x}
          y1={0}
          x2={x}
          y2={height}
          stroke="#1b212c"
          strokeWidth="1"
        />
      );
    })}
    {/* Ápices detectados: ancoram a leitura do gráfico no traçado real. */}
    {corners.map((corner) => {
      const x = (corner.apex_distance_m / totalDistance) * CHART_WIDTH;
      return (
        <line
          key={`${keyPrefix}-corner-${corner.label}`}
          x1={x}
          y1={0}
          x2={x}
          y2={height}
          stroke="#475569"
          strokeWidth="1"
          strokeDasharray="2 4"
        />
      );
    })}
  </>
));
Overlay.displayName = 'Overlay';

export const TelemetryPlots: React.FC = () => {
  const comparison = useTelemetryStore((state) => state.comparison);
  const hoveredDistanceM = useTelemetryStore((state) => state.hoveredDistanceM);
  const setHoveredDistanceM = useTelemetryStore((state) => state.setHoveredDistanceM);
  const activeInsightId = useTelemetryStore((state) => state.activeInsightId);
  const containerRef = useRef<HTMLDivElement>(null);

  const data = comparison.data;
  const channels = data?.channels ?? [];
  const totalDistance = data?.total_distance_m ?? 0;
  const gridStep = data?.grid_step_m ?? 5;

  /** Escalas derivadas dos dados reais: nada de teto fixo em 350 km/h. */
  const scales = useMemo(() => {
    if (channels.length === 0) return null;

    let speedMax = 0;
    let deltaMin = 0;
    let deltaMax = 0;
    for (const point of channels) {
      speedMax = Math.max(speedMax, point.ref.speed_kmh, point.comp.speed_kmh);
      deltaMin = Math.min(deltaMin, point.delta_time_s);
      deltaMax = Math.max(deltaMax, point.delta_time_s);
    }

    const [speedLow, speedHigh] = niceBounds(0, speedMax, 50);
    const [deltaLow, deltaHigh] = niceBounds(deltaMin, deltaMax, 0.1);

    return {
      speedTicks: Array.from(
        { length: Math.floor((speedHigh - speedLow) / 50) },
        (_, index) => speedLow + (index + 1) * 50
      ).filter((tick) => tick < speedHigh),
      speedToY: (value: number) =>
        SPEED_HEIGHT - ((value - speedLow) / (speedHigh - speedLow)) * SPEED_HEIGHT,
      deltaLow,
      deltaHigh,
      deltaToY: (value: number) =>
        DELTA_HEIGHT - ((value - deltaLow) / (deltaHigh - deltaLow)) * DELTA_HEIGHT,
      pedalToY: (value: number) => PEDALS_HEIGHT - (value / 100) * PEDALS_HEIGHT
    };
  }, [channels]);

  /** Traçados geométricos estáticos: calculados apenas na troca de dados, não a cada tick do replay. */
  const paths = useMemo(() => {
    if (!scales || channels.length === 0 || totalDistance <= 0) return null;

    const speedRefPath = buildPath(channels, totalDistance, (p) => p.ref.speed_kmh, scales.speedToY);
    const speedCompPath = buildPath(channels, totalDistance, (p) => p.comp.speed_kmh, scales.speedToY);
    const deltaPath = buildPath(channels, totalDistance, (p) => p.delta_time_s, scales.deltaToY);
    const throttleRefPath = buildPath(channels, totalDistance, (p) => p.ref.throttle_pct, scales.pedalToY);
    const throttleCompPath = buildPath(channels, totalDistance, (p) => p.comp.throttle_pct, scales.pedalToY);
    const brakeRefPath = buildPath(channels, totalDistance, (p) => p.ref.brake_pct, scales.pedalToY);
    const brakeCompPath = buildPath(channels, totalDistance, (p) => p.comp.brake_pct, scales.pedalToY);

    const deltaZeroY = scales.deltaToY(0);
    const deltaAreaPath = `M 0,${deltaZeroY} ${channels
      .map((point) => {
        const x = (point.distance_m / totalDistance) * CHART_WIDTH;
        return `L ${x.toFixed(1)},${scales.deltaToY(point.delta_time_s).toFixed(1)}`;
      })
      .join(' ')} L ${CHART_WIDTH},${deltaZeroY} Z`;

    return {
      speedRefPath,
      speedCompPath,
      deltaPath,
      throttleRefPath,
      throttleCompPath,
      brakeRefPath,
      brakeCompPath,
      deltaAreaPath,
      deltaZeroY
    };
  }, [channels, scales, totalDistance]);

  const distanceTicks = useMemo(() => {
    if (totalDistance <= 0) return [];
    const interval = totalDistance > 6000 ? 1000 : 500;
    const ticks: number[] = [];
    for (let distance = interval; distance < totalDistance; distance += interval) {
      ticks.push(distance);
    }
    return ticks;
  }, [totalDistance]);

  const activeIndex = useMemo(() => {
    if (channels.length === 0) return 0;
    const distance = hoveredDistanceM ?? 0;
    const index = Math.round((distance - channels[0].distance_m) / gridStep);
    return Math.max(0, Math.min(channels.length - 1, index));
  }, [channels, hoveredDistanceM, gridStep]);

  const activePoint = channels[activeIndex];
  const activeInsight = data?.insights.find((insight) => insight.id === activeInsightId);

  const handleMouseMove = useCallback(
    (event: React.MouseEvent<HTMLDivElement>) => {
      if (!containerRef.current || totalDistance <= 0) return;
      const rect = containerRef.current.getBoundingClientRect();
      const fraction = Math.max(0, Math.min(1, (event.clientX - rect.left) / rect.width));
      setHoveredDistanceM(Math.round((fraction * totalDistance) / gridStep) * gridStep);
    },
    [gridStep, setHoveredDistanceM, totalDistance]
  );

  if (comparison.loading) {
    return (
      <div className="flex flex-1 flex-col items-center justify-center gap-3 bg-[#0a0c10] font-mono text-xs text-neutral-500">
        <Loader2 className="h-6 w-6 animate-spin text-sky-500" />
        <span>Buscando a telemetria das duas voltas e alinhando na grade espacial…</span>
      </div>
    );
  }

  if (comparison.error) {
    return (
      <div className="flex flex-1 flex-col items-center justify-center gap-3 bg-[#0a0c10] px-8 text-center font-mono">
        <AlertTriangle className="h-7 w-7 text-amber-500" />
        <span className="text-sm font-bold text-amber-400">{comparison.error.code}</span>
        <p className="max-w-lg text-xs leading-relaxed text-neutral-400">{comparison.error.message}</p>
      </div>
    );
  }

  if (!data || !scales || !paths || !activePoint || totalDistance <= 0) {
    return (
      <div className="flex flex-1 flex-col items-center justify-center gap-2 bg-[#0a0c10] px-8 text-center font-mono text-xs text-neutral-600">
        <span className="text-neutral-500">Nenhuma comparação ativa.</span>
        <span>Escolha uma sessão, dois pilotos e duas voltas cronometradas no painel à esquerda.</span>
      </div>
    );
  }

  const refCode = data.reference_lap.driver_code;
  const compCode = data.comparison_lap.driver_code;
  const cursorX = (activePoint.distance_m / totalDistance) * CHART_WIDTH;

  const insightStartX = activeInsight
    ? (activeInsight.distance_start_m / totalDistance) * CHART_WIDTH
    : 0;
  const insightWidth = activeInsight
    ? Math.max(2, ((activeInsight.distance_end_m - activeInsight.distance_start_m) / totalDistance) * CHART_WIDTH)
    : 0;

  return (
    <div className="flex h-full flex-1 select-none flex-col overflow-y-auto bg-[#0a0c10]">
      <div className="grid grid-cols-2 gap-2 border-b border-[#212836] bg-[#0d1015] p-3 sm:grid-cols-4 lg:grid-cols-7">
        <MetricInstrument
          label="Distância"
          value={`${Math.round(activePoint.distance_m)}`}
          unit="m"
          annotation={`${Math.round((activePoint.distance_m / totalDistance) * 100)}%`}
        />
        <MetricInstrument
          label="Delta"
          value={`${activePoint.delta_time_s >= 0 ? '+' : ''}${activePoint.delta_time_s.toFixed(3)}`}
          unit="s"
          variant={activePoint.delta_time_s > 0 ? 'loss' : 'gain'}
          annotation={activePoint.delta_time_s > 0 ? `${compCode} atrás` : `${compCode} à frente`}
        />
        <MetricInstrument
          label="Velocidade"
          value={activePoint.ref.speed_kmh.toFixed(1)}
          unit="km/h"
          secondaryValue={activePoint.comp.speed_kmh.toFixed(1)}
          secondaryLabel={compCode}
        />
        <MetricInstrument
          label="Acelerador"
          value={`${Math.round(activePoint.ref.throttle_pct)}%`}
          secondaryValue={`${Math.round(activePoint.comp.throttle_pct)}%`}
          secondaryLabel={compCode}
        />
        <MetricInstrument
          label="Freio"
          value={`${Math.round(activePoint.ref.brake_pct)}%`}
          secondaryValue={`${Math.round(activePoint.comp.brake_pct)}%`}
          secondaryLabel={compCode}
          variant={activePoint.ref.brake_pct > 0 ? 'loss' : 'default'}
        />
        <MetricInstrument
          label="Marcha"
          value={`G${activePoint.ref.gear}`}
          secondaryValue={`G${activePoint.comp.gear}`}
          secondaryLabel={compCode}
        />
        <MetricInstrument
          label="DRS"
          value={activePoint.ref.drs ? 'ABERTO' : 'FECHADO'}
          variant={activePoint.ref.drs ? 'gain' : 'default'}
          secondaryValue={activePoint.comp.drs ? 'ON' : 'OFF'}
          secondaryLabel={compCode}
        />
      </div>

      <div
        ref={containerRef}
        onMouseMove={handleMouseMove}
        className="relative flex flex-1 cursor-crosshair flex-col space-y-4 p-4"
      >
        <div className="relative border border-[#212836] bg-[#0d1015] p-3">
          <div className="mb-1 flex items-center justify-between font-mono text-[11px] text-neutral-400">
            <span className="font-semibold text-neutral-200">DELTA ACUMULADO POR DISTÂNCIA · ΔT(d)</span>
            <div className="flex items-center space-x-3 text-[10px]">
              <span className="text-rose-400">▲ {compCode} perde</span>
              <span className="text-emerald-400">▼ {compCode} ganha</span>
              <span className="text-neutral-500">
                {scales.deltaLow.toFixed(1)} … {scales.deltaHigh.toFixed(1)} s
              </span>
            </div>
          </div>
          <div className="relative h-20 w-full">
            <svg
              data-testid="chart-delta"
              viewBox={`0 0 ${CHART_WIDTH} ${DELTA_HEIGHT}`}
              preserveAspectRatio="none"
              className="h-full w-full"
            >
              <Overlay
                height={DELTA_HEIGHT}
                keyPrefix="delta"
                activeInsight={activeInsight}
                insightStartX={insightStartX}
                insightWidth={insightWidth}
                distanceTicks={distanceTicks}
                corners={data.corners}
                totalDistance={totalDistance}
              />
              <line x1={0} y1={paths.deltaZeroY} x2={CHART_WIDTH} y2={paths.deltaZeroY} stroke="#374151" strokeDasharray="3 3" />
              <path d={paths.deltaAreaPath} fill="#ef4444" fillOpacity="0.15" />
              <path d={paths.deltaPath} fill="none" stroke="#f87171" strokeWidth="2" />
              <line x1={cursorX} y1={0} x2={cursorX} y2={DELTA_HEIGHT} stroke="#38bdf8" strokeWidth="1.5" />
            </svg>
          </div>
        </div>

        <div className="relative border border-[#212836] bg-[#0d1015] p-3">
          <div className="mb-1 flex items-center justify-between font-mono text-[11px] text-neutral-400">
            <span className="font-semibold text-neutral-200">VELOCIDADE · km/h</span>
            <div className="flex items-center space-x-3 text-[10px]">
              <span style={{ color: data.reference_lap.team_colour }}>
                ― {refCode} #{data.reference_lap.driver_number}
              </span>
              <span style={{ color: data.comparison_lap.team_colour }}>
                ― {compCode} #{data.comparison_lap.driver_number}
              </span>
            </div>
          </div>
          <div className="relative h-32 w-full">
            <svg
              data-testid="chart-speed"
              viewBox={`0 0 ${CHART_WIDTH} ${SPEED_HEIGHT}`}
              preserveAspectRatio="none"
              className="h-full w-full"
            >
              <Overlay
                height={SPEED_HEIGHT}
                keyPrefix="speed"
                activeInsight={activeInsight}
                insightStartX={insightStartX}
                insightWidth={insightWidth}
                distanceTicks={distanceTicks}
                corners={data.corners}
                totalDistance={totalDistance}
              />
              {scales.speedTicks.map((tick) => (
                <line
                  key={`speed-tick-${tick}`}
                  x1={0}
                  y1={scales.speedToY(tick)}
                  x2={CHART_WIDTH}
                  y2={scales.speedToY(tick)}
                  stroke="#1a202c"
                  strokeDasharray="2 2"
                />
              ))}
              <path d={paths.speedRefPath} fill="none" stroke={data.reference_lap.team_colour} strokeWidth="2" />
              <path
                d={paths.speedCompPath}
                fill="none"
                stroke={data.comparison_lap.team_colour}
                strokeWidth="1.8"
                strokeDasharray="4 2"
              />
              <line x1={cursorX} y1={0} x2={cursorX} y2={SPEED_HEIGHT} stroke="#38bdf8" strokeWidth="1.5" />
            </svg>
          </div>
        </div>

        <div className="relative border border-[#212836] bg-[#0d1015] p-3">
          <div className="mb-1 flex items-center justify-between font-mono text-[11px] text-neutral-400">
            <span className="font-semibold text-neutral-200">ACELERADOR &amp; FREIO · %</span>
            <div className="flex items-center space-x-3 text-[10px]">
              <span className="text-emerald-400">― acelerador {refCode}</span>
              <span className="text-amber-300">― acelerador {compCode}</span>
              <span className="text-rose-400">― freio</span>
            </div>
          </div>
          <div className="relative h-24 w-full">
            <svg
              data-testid="chart-pedals"
              viewBox={`0 0 ${CHART_WIDTH} ${PEDALS_HEIGHT}`}
              preserveAspectRatio="none"
              className="h-full w-full"
            >
              <Overlay
                height={PEDALS_HEIGHT}
                keyPrefix="pedals"
                activeInsight={activeInsight}
                insightStartX={insightStartX}
                insightWidth={insightWidth}
                distanceTicks={distanceTicks}
                corners={data.corners}
                totalDistance={totalDistance}
              />
              <line x1={0} y1={PEDALS_HEIGHT / 2} x2={CHART_WIDTH} y2={PEDALS_HEIGHT / 2} stroke="#1a202c" strokeDasharray="2 2" />
              <path d={paths.brakeRefPath} fill="none" stroke="#ef4444" strokeWidth="2" />
              <path d={paths.brakeCompPath} fill="none" stroke="#f87171" strokeWidth="1.5" strokeDasharray="3 2" />
              <path d={paths.throttleRefPath} fill="none" stroke="#22c55e" strokeWidth="2" />
              <path d={paths.throttleCompPath} fill="none" stroke="#facc15" strokeWidth="1.5" strokeDasharray="4 2" />
              <line x1={cursorX} y1={0} x2={cursorX} y2={PEDALS_HEIGHT} stroke="#38bdf8" strokeWidth="1.5" />
            </svg>
          </div>
        </div>

        <div className="flex items-center justify-between px-1 font-mono text-[10px] text-neutral-500">
          <span>0 m</span>
          {distanceTicks
            .filter((_, index) => index % 2 === 0)
            .map((distance) => (
              <span key={`label-${distance}`}>{distance.toLocaleString('pt-BR')} m</span>
            ))}
          <span>{Math.round(totalDistance).toLocaleString('pt-BR')} m</span>
        </div>
      </div>
    </div>
  );
};
