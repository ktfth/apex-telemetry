'use client';

import React, { useEffect } from 'react';
import { useTelemetryStore } from '../store/telemetryStore';
import { useLiveSession } from '../hooks/useLiveSession';
import { ErrorBoundary } from '../components/ErrorBoundary';
import { Header } from '../components/Header';
import { LapSelector } from '../components/LapSelector';
import { TelemetryPlots } from '../components/TelemetryPlots';
import { InsightsPanel } from '../components/InsightsPanel';
import { BottomPanel } from '../components/BottomPanel';

export default function DashboardPage() {
  const sessionKey = useTelemetryStore((state) => state.sessionKey);
  const refDriverNumber = useTelemetryStore((state) => state.refDriverNumber);
  const refLapNumber = useTelemetryStore((state) => state.refLapNumber);
  const compDriverNumber = useTelemetryStore((state) => state.compDriverNumber);
  const compLapNumber = useTelemetryStore((state) => state.compLapNumber);
  const refreshComparison = useTelemetryStore((state) => state.refreshComparison);
  const refreshCircuit = useTelemetryStore((state) => state.refreshCircuit);
  const leftPanelOpen = useTelemetryStore((state) => state.leftPanelOpen);
  const rightPanelOpen = useTelemetryStore((state) => state.rightPanelOpen);
  const bottomPanelOpen = useTelemetryStore((state) => state.bottomPanelOpen);

  // O replay consome o stream do gateway; nada é animado localmente.
  useLiveSession();

  // Recalcula a comparação sempre que a seleção estiver completa e mudar.
  useEffect(() => {
    void refreshComparison();
  }, [sessionKey, refDriverNumber, refLapNumber, compDriverNumber, compLapNumber, refreshComparison]);

  // O traçado depende apenas da sessão e é reaproveitado entre comparações.
  useEffect(() => {
    void refreshCircuit();
  }, [sessionKey, refreshCircuit]);

  return (
    <ErrorBoundary>
      <div className="flex h-screen w-screen flex-col overflow-hidden bg-[#0a0c10]">
        <Header />

        <main className="flex min-h-0 flex-1 overflow-hidden">
          {leftPanelOpen && <LapSelector />}
          <TelemetryPlots />
          {rightPanelOpen && <InsightsPanel />}
        </main>

        {bottomPanelOpen && <BottomPanel />}
      </div>
    </ErrorBoundary>
  );
}
