'use client';

import React, { useEffect, useRef } from 'react';
import { useTelemetryStore } from '../store/telemetryStore';
import { useLiveSession } from '../hooks/useLiveSession';
import { ErrorBoundary } from '../components/ErrorBoundary';
import { Header } from '../components/Header';
import { LapSelector } from '../components/LapSelector';
import { TelemetryPlots } from '../components/TelemetryPlots';
import { InsightsPanel } from '../components/InsightsPanel';
import { BottomPanel } from '../components/BottomPanel';
import { TelemetrySkeleton } from '../components/Skeletons';

export default function DashboardPage() {
  const {
    leftPanelOpen,
    rightPanelOpen,
    bottomPanelOpen,
    comparisonLoading,
    refDriverNumber,
    refLapNumber,
    compDriverNumber,
    compLapNumber,
    refreshComparison,
    replayActive,
    replaySpeed,
    comparison,
    setHoveredDistanceM
  } = useTelemetryStore();

  // Inicia escuta passiva de SSE no backend
  useLiveSession(9472, true);

  // Recalcula comparação ao mudar seleção
  useEffect(() => {
    void refreshComparison();
  }, [refDriverNumber, refLapNumber, compDriverNumber, compLapNumber, refreshComparison]);

  // Loop suave de animação para o modo Replay
  const animFrameRef = useRef<number | null>(null);
  const currentDistRef = useRef<number>(0);

  useEffect(() => {
    if (!replayActive) {
      if (animFrameRef.current !== null) {
        cancelAnimationFrame(animFrameRef.current);
        animFrameRef.current = null;
      }
      return;
    }

    let lastTime = performance.now();
    const totalDist = comparison.total_distance_m || 5412.0;

    const tick = (now: number) => {
      const dt = (now - lastTime) / 1000.0;
      lastTime = now;

      // Avança proporcionalmente à velocidade média simulada (250 km/h = ~70 m/s)
      const deltaDist = 70.0 * replaySpeed * dt;
      currentDistRef.current = (currentDistRef.current + deltaDist) % totalDist;
      setHoveredDistanceM(Math.round(currentDistRef.current));

      animFrameRef.current = requestAnimationFrame(tick);
    };

    animFrameRef.current = requestAnimationFrame(tick);

    return () => {
      if (animFrameRef.current !== null) {
        cancelAnimationFrame(animFrameRef.current);
        animFrameRef.current = null;
      }
    };
  }, [replayActive, replaySpeed, comparison.total_distance_m, setHoveredDistanceM]);

  return (
    <ErrorBoundary>
      <div className="flex flex-col h-screen w-screen overflow-hidden bg-[#0a0c10]">
        {/* Top Header */}
        <Header />

        {/* Main Analysis Workspace */}
        <main className="flex-1 flex overflow-hidden min-h-0">
          {/* Left Column: Selection Matrix */}
          {leftPanelOpen && <LapSelector />}

          {/* Central Dominant Column: Distance Synchronized Telemetry Plots */}
          {comparisonLoading ? <TelemetrySkeleton /> : <TelemetryPlots />}

          {/* Right Column: Circuit Vector Map & Explainable Evidence Insights */}
          {rightPanelOpen && <InsightsPanel />}
        </main>

        {/* Bottom Expandable Tray: Race Control, Stints, Tyre Degradation */}
        {bottomPanelOpen && <BottomPanel />}
      </div>
    </ErrorBoundary>
  );
}
