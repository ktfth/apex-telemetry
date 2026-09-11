'use client';

import React from 'react';
import { useTelemetryStore } from '../store/telemetryStore';
import { Header } from '../components/Header';
import { LapSelector } from '../components/LapSelector';
import { TelemetryPlots } from '../components/TelemetryPlots';
import { InsightsPanel } from '../components/InsightsPanel';
import { BottomPanel } from '../components/BottomPanel';

export default function DashboardPage() {
  const { leftPanelOpen, rightPanelOpen, bottomPanelOpen } = useTelemetryStore();

  return (
    <div className="flex flex-col h-screen w-screen overflow-hidden bg-[#0a0c10]">
      {/* Top Header */}
      <Header />

      {/* Main Analysis Workspace */}
      <main className="flex-1 flex overflow-hidden min-h-0">
        {/* Left Column: Selection Matrix */}
        {leftPanelOpen && <LapSelector />}

        {/* Central Dominant Column: Distance Synchronized Telemetry Plots */}
        <TelemetryPlots />

        {/* Right Column: Circuit Vector Map & Explainable Evidence Insights */}
        {rightPanelOpen && <InsightsPanel />}
      </main>

      {/* Bottom Expandable Tray: Race Control, Stints, Tyre Degradation */}
      {bottomPanelOpen && <BottomPanel />}
    </div>
  );
}
