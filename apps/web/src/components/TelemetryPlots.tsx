'use client';

import React, { useRef, useCallback } from 'react';
import { useTelemetryStore } from '../store/telemetryStore';
import { DEMO_LAP_COMPARISON } from '../fixtures/demoBahrain2024';
import { MetricInstrument } from '@apex-telemetry/ui';

export const TelemetryPlots: React.FC = () => {
  const { hoveredDistanceM, setHoveredDistanceM, activeInsightId } = useTelemetryStore();
  const comparison = DEMO_LAP_COMPARISON;
  const channels = comparison.channels;
  const totalDistance = comparison.total_distance_m;

  const containerRef = useRef<HTMLDivElement>(null);

  // Ponto atualmente selecionado pelo cursor
  const activeDistance = hoveredDistanceM !== null ? hoveredDistanceM : 1550; // default no ápice de T4
  const activeIndex = Math.min(
    channels.length - 1,
    Math.max(0, Math.round(activeDistance / comparison.grid_step_m))
  );
  const activePoint = channels[activeIndex] || channels[0];

  // Insight ativo para sombreamento do trecho
  const activeInsight = comparison.insights.find((i) => i.id === activeInsightId);

  // Mouse move handler
  const handleMouseMove = useCallback(
    (e: React.MouseEvent<HTMLDivElement>) => {
      if (!containerRef.current) return;
      const rect = containerRef.current.getBoundingClientRect();
      const clientX = e.clientX - rect.left;
      const pct = Math.max(0, Math.min(1, clientX / rect.width));
      const dist = Math.round((pct * totalDistance) / comparison.grid_step_m) * comparison.grid_step_m;
      setHoveredDistanceM(dist);
    },
    [comparison.grid_step_m, setHoveredDistanceM, totalDistance]
  );

  const handleMouseLeave = useCallback(() => {
    // Mantém o último ponto ou reseta para o trecho do insight ativo
    if (activeInsight) {
      setHoveredDistanceM(activeInsight.distance_start_m);
    }
  }, [activeInsight, setHoveredDistanceM]);

  // Projeções SVG para gráficos
  const chartWidth = 1000;
  const cursorX = (activePoint.distance_m / totalDistance) * chartWidth;

  // Insight highlight zone
  const insightStartX = activeInsight ? (activeInsight.distance_start_m / totalDistance) * chartWidth : 0;
  const insightEndX = activeInsight ? (activeInsight.distance_end_m / totalDistance) * chartWidth : 0;
  const insightWidth = Math.max(2, insightEndX - insightStartX);

  // Gera SVG paths para velocidade (0-350 km/h mapeado para altura 120px)
  const speedH = 120;
  const speedRefPath = channels
    .map((c, i) => {
      const x = (c.distance_m / totalDistance) * chartWidth;
      const y = speedH - (c.ref.speed_kmh / 350) * speedH;
      return `${i === 0 ? 'M' : 'L'} ${x.toFixed(1)},${y.toFixed(1)}`;
    })
    .join(' ');

  const speedCompPath = channels
    .map((c, i) => {
      const x = (c.distance_m / totalDistance) * chartWidth;
      const y = speedH - (c.comp.speed_kmh / 350) * speedH;
      return `${i === 0 ? 'M' : 'L'} ${x.toFixed(1)},${y.toFixed(1)}`;
    })
    .join(' ');

  // Gera SVG paths para Delta de Tempo (-0.1s a +0.5s mapeado para 70px)
  const deltaH = 70;
  const deltaZeroY = 25; // zero line
  const deltaScale = 80; // pixels per second
  const deltaPoints = channels.map((c) => {
    const x = (c.distance_m / totalDistance) * chartWidth;
    const y = Math.max(2, Math.min(deltaH - 2, deltaZeroY + c.delta_time_s * deltaScale));
    return { x, y };
  });

  const deltaLinePath = deltaPoints
    .map((p, i) => `${i === 0 ? 'M' : 'L'} ${p.x.toFixed(1)},${p.y.toFixed(1)}`)
    .join(' ');

  const deltaAreaPath = `M 0,${deltaZeroY} ` +
    deltaPoints.map((p) => `L ${p.x.toFixed(1)},${p.y.toFixed(1)}`).join(' ') +
    ` L ${chartWidth},${deltaZeroY} Z`;

  // Throttle & Brake (0-100% mapeado para 80px)
  const pedalsH = 80;
  const throttleRefPath = channels
    .map((c, i) => {
      const x = (c.distance_m / totalDistance) * chartWidth;
      const y = pedalsH - (c.ref.throttle_pct / 100) * pedalsH;
      return `${i === 0 ? 'M' : 'L'} ${x.toFixed(1)},${y.toFixed(1)}`;
    })
    .join(' ');

  const throttleCompPath = channels
    .map((c, i) => {
      const x = (c.distance_m / totalDistance) * chartWidth;
      const y = pedalsH - (c.comp.throttle_pct / 100) * pedalsH;
      return `${i === 0 ? 'M' : 'L'} ${x.toFixed(1)},${y.toFixed(1)}`;
    })
    .join(' ');

  const brakeRefPath = channels
    .map((c, i) => {
      const x = (c.distance_m / totalDistance) * chartWidth;
      const y = pedalsH - (c.ref.brake_pct / 100) * pedalsH;
      return `${i === 0 ? 'M' : 'L'} ${x.toFixed(1)},${y.toFixed(1)}`;
    })
    .join(' ');

  const brakeCompPath = channels
    .map((c, i) => {
      const x = (c.distance_m / totalDistance) * chartWidth;
      const y = pedalsH - (c.comp.brake_pct / 100) * pedalsH;
      return `${i === 0 ? 'M' : 'L'} ${x.toFixed(1)},${y.toFixed(1)}`;
    })
    .join(' ');

  // Grid vertical a cada 500m
  const distanceTicks = [];
  for (let d = 500; d < totalDistance; d += 500) {
    distanceTicks.push(d);
  }

  return (
    <div className="flex-1 flex flex-col h-full bg-[#0a0c10] select-none overflow-y-auto">
      {/* Barra de Instrumentação em Tempo Real (Valores sob o Crosshair) */}
      <div className="grid grid-cols-2 sm:grid-cols-4 lg:grid-cols-7 gap-2 p-3 bg-[#0d1015] border-b border-[#212836]">
        <MetricInstrument
          label="Distância"
          value={`${activePoint.distance_m}`}
          unit="m"
          annotation={`${~~((activePoint.distance_m / totalDistance) * 100)}%`}
        />
        <MetricInstrument
          label="Delta Tempo"
          value={`${activePoint.delta_time_s >= 0 ? '+' : ''}${activePoint.delta_time_s.toFixed(3)}`}
          unit="s"
          variant={activePoint.delta_time_s > 0 ? 'loss' : 'gain'}
          annotation={activePoint.delta_time_s > 0 ? 'LEC ATRÁS' : 'LEC À FRENTE'}
        />
        <MetricInstrument
          label="Velocidade"
          value={`${activePoint.ref.speed_kmh.toFixed(1)}`}
          unit="km/h"
          secondaryValue={`${activePoint.comp.speed_kmh.toFixed(1)}`}
          secondaryLabel="LEC"
        />
        <MetricInstrument
          label="Acelerador"
          value={`${activePoint.ref.throttle_pct}%`}
          secondaryValue={`${activePoint.comp.throttle_pct}%`}
          secondaryLabel="LEC"
        />
        <MetricInstrument
          label="Freio"
          value={`${activePoint.ref.brake_pct}%`}
          secondaryValue={`${activePoint.comp.brake_pct}%`}
          secondaryLabel="LEC"
          variant={activePoint.ref.brake_pct > 0 ? 'loss' : 'default'}
        />
        <MetricInstrument
          label="Marcha"
          value={`G${activePoint.ref.gear}`}
          secondaryValue={`G${activePoint.comp.gear}`}
          secondaryLabel="LEC"
        />
        <MetricInstrument
          label="DRS"
          value={activePoint.ref.drs ? 'ATIVO' : 'FECHADO'}
          variant={activePoint.ref.drs ? 'gain' : 'default'}
          secondaryValue={activePoint.comp.drs ? 'ON' : 'OFF'}
          secondaryLabel="LEC"
        />
      </div>

      {/* Área Interativa dos Gráficos Sincronizados */}
      <div
        ref={containerRef}
        onMouseMove={handleMouseMove}
        onMouseLeave={handleMouseLeave}
        className="flex-1 p-4 flex flex-col space-y-4 cursor-crosshair relative"
      >
        {/* Gráfico 1: Delta de Tempo por Distância */}
        <div className="border border-[#212836] bg-[#0d1015] p-3 relative">
          <div className="flex items-center justify-between text-[11px] font-mono text-neutral-400 mb-1">
            <span className="font-semibold text-neutral-200">DELTA ACUMULADO POR DISTÂNCIA [ΔT(d)]</span>
            <div className="flex items-center space-x-3 text-[10px]">
              <span className="text-sky-400">■ VER BASE</span>
              <span className="text-rose-400">▲ LEC PERDA (+s)</span>
              <span className="text-emerald-400">▼ LEC GANHO (-s)</span>
            </div>
          </div>

          <div className="h-20 w-full relative">
            <svg viewBox={`0 0 ${chartWidth} ${deltaH}`} preserveAspectRatio="none" className="w-full h-full">
              {/* Sombreamento do Insight Ativo */}
              {activeInsight && (
                <rect
                  x={insightStartX}
                  y={0}
                  width={insightWidth}
                  height={deltaH}
                  fill="#eab308"
                  fillOpacity="0.12"
                />
              )}

              {/* Grid vertical */}
              {distanceTicks.map((d) => {
                const x = (d / totalDistance) * chartWidth;
                return <line key={`dg-${d}`} x1={x} y1={0} x2={x} y2={deltaH} stroke="#1b212c" strokeWidth="1" />;
              })}

              {/* Zero baseline */}
              <line x1={0} y1={deltaZeroY} x2={chartWidth} y2={deltaZeroY} stroke="#374151" strokeDasharray="3 3" />

              {/* Area e Linha de Delta */}
              <path d={deltaAreaPath} fill="#ef4444" fillOpacity="0.15" />
              <path d={deltaLinePath} fill="none" stroke="#f87171" strokeWidth="2" />

              {/* Cursor vertical */}
              <line x1={cursorX} y1={0} x2={cursorX} y2={deltaH} stroke="#38bdf8" strokeWidth="1.5" />
            </svg>
          </div>
        </div>

        {/* Gráfico 2: Velocidade Sincronizada (Speed km/h) */}
        <div className="border border-[#212836] bg-[#0d1015] p-3 relative">
          <div className="flex items-center justify-between text-[11px] font-mono text-neutral-400 mb-1">
            <span className="font-semibold text-neutral-200">VELOCIDADE [km/h]</span>
            <div className="flex items-center space-x-3 text-[10px]">
              <span className="text-sky-400">― VER (#1)</span>
              <span className="text-rose-400">― LEC (#16)</span>
            </div>
          </div>

          <div className="h-32 w-full relative">
            <svg viewBox={`0 0 ${chartWidth} ${speedH}`} preserveAspectRatio="none" className="w-full h-full">
              {/* Insight highlight */}
              {activeInsight && (
                <rect
                  x={insightStartX}
                  y={0}
                  width={insightWidth}
                  height={speedH}
                  fill="#eab308"
                  fillOpacity="0.12"
                />
              )}

              {/* Grid vertical */}
              {distanceTicks.map((d) => {
                const x = (d / totalDistance) * chartWidth;
                return <line key={`sg-${d}`} x1={x} y1={0} x2={x} y2={speedH} stroke="#1b212c" strokeWidth="1" />;
              })}

              {/* Linhas de grade de velocidade (100, 200, 300 km/h) */}
              <line x1={0} y1={speedH - (100 / 350) * speedH} x2={chartWidth} y2={speedH - (100 / 350) * speedH} stroke="#1a202c" strokeDasharray="2 2" />
              <line x1={0} y1={speedH - (200 / 350) * speedH} x2={chartWidth} y2={speedH - (200 / 350) * speedH} stroke="#1a202c" strokeDasharray="2 2" />
              <line x1={0} y1={speedH - (300 / 350) * speedH} x2={chartWidth} y2={speedH - (300 / 350) * speedH} stroke="#1a202c" strokeDasharray="2 2" />

              {/* Curvas */}
              <path d={speedRefPath} fill="none" stroke="#38bdf8" strokeWidth="2" />
              <path d={speedCompPath} fill="none" stroke="#f87171" strokeWidth="1.8" strokeDasharray="4 2" />

              {/* Cursor vertical */}
              <line x1={cursorX} y1={0} x2={cursorX} y2={speedH} stroke="#38bdf8" strokeWidth="1.5" />
            </svg>
          </div>
        </div>

        {/* Gráfico 3: Acelerador e Freio (Pedais %) */}
        <div className="border border-[#212836] bg-[#0d1015] p-3 relative">
          <div className="flex items-center justify-between text-[11px] font-mono text-neutral-400 mb-1">
            <span className="font-semibold text-neutral-200">ACELERADOR & FREIO [%]</span>
            <div className="flex items-center space-x-3 text-[10px]">
              <span className="text-emerald-400">― THROTTLE VER</span>
              <span className="text-amber-300">― THROTTLE LEC</span>
              <span className="text-rose-400">― BRAKE</span>
            </div>
          </div>

          <div className="h-24 w-full relative">
            <svg viewBox={`0 0 ${chartWidth} ${pedalsH}`} preserveAspectRatio="none" className="w-full h-full">
              {/* Insight highlight */}
              {activeInsight && (
                <rect
                  x={insightStartX}
                  y={0}
                  width={insightWidth}
                  height={pedalsH}
                  fill="#eab308"
                  fillOpacity="0.12"
                />
              )}

              {/* Grid vertical */}
              {distanceTicks.map((d) => {
                const x = (d / totalDistance) * chartWidth;
                return <line key={`pg-${d}`} x1={x} y1={0} x2={x} y2={pedalsH} stroke="#1b212c" strokeWidth="1" />;
              })}

              {/* 50% e 100% lines */}
              <line x1={0} y1={pedalsH / 2} x2={chartWidth} y2={pedalsH / 2} stroke="#1a202c" strokeDasharray="2 2" />

              {/* Curvas de Freio */}
              <path d={brakeRefPath} fill="none" stroke="#ef4444" strokeWidth="2" />
              <path d={brakeCompPath} fill="none" stroke="#f87171" strokeWidth="1.5" strokeDasharray="3 2" />

              {/* Curvas de Acelerador */}
              <path d={throttleRefPath} fill="none" stroke="#22c55e" strokeWidth="2" />
              <path d={throttleCompPath} fill="none" stroke="#facc15" strokeWidth="1.5" strokeDasharray="4 2" />

              {/* Cursor vertical */}
              <line x1={cursorX} y1={0} x2={cursorX} y2={pedalsH} stroke="#38bdf8" strokeWidth="1.5" />
            </svg>
          </div>
        </div>

        {/* Eixo de Distância (Régua com Ticks) */}
        <div className="flex items-center justify-between text-[10px] font-mono text-neutral-500 px-1">
          <span>0 m</span>
          <span>1.000 m</span>
          <span>2.000 m</span>
          <span>3.000 m</span>
          <span>4.000 m</span>
          <span>5.000 m</span>
          <span>{totalDistance} m</span>
        </div>
      </div>
    </div>
  );
};
