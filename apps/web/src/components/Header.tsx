'use client';

import React from 'react';
import { useTelemetryStore } from '../store/telemetryStore';
import { DEMO_SOURCE_INFO } from '../fixtures/demoBahrain2024';
import { PanelLeft, PanelRight, PanelBottom, ShieldAlert, Cpu } from 'lucide-react';

export const Header: React.FC = () => {
  const {
    leftPanelOpen,
    rightPanelOpen,
    bottomPanelOpen,
    toggleLeftPanel,
    toggleRightPanel,
    toggleBottomPanel
  } = useTelemetryStore();

  return (
    <header className="h-12 border-b border-[#212836] bg-[#0c0e12] px-4 flex items-center justify-between select-none shrink-0 z-30">
      {/* Brand & Session Context */}
      <div className="flex items-center space-x-4">
        <div className="flex items-center space-x-2">
          <div className="w-2.5 h-2.5 bg-sky-400 rounded-none transform rotate-45" />
          <span className="font-mono text-sm font-bold tracking-wider text-neutral-100 uppercase">
            Apex<span className="text-sky-400">Telemetry</span>
          </span>
          <span className="text-[10px] font-mono px-1.5 py-0.5 bg-[#161b24] text-neutral-400 border border-[#232936]">
            v1.0.0-fase1
          </span>
        </div>

        <div className="h-4 w-px bg-[#212836]" />

        {/* Breadcrumbs de Sessão */}
        <div className="flex items-center space-x-2 font-mono text-xs text-neutral-400">
          <span className="text-neutral-500">2024</span>
          <span>/</span>
          <span className="text-neutral-300 font-medium">Bahrain GP</span>
          <span>/</span>
          <span className="px-1.5 py-0.5 bg-neutral-800 text-neutral-200 text-[10px] font-semibold tracking-wide">
            QUALIFYING Q3
          </span>
        </div>
      </div>

      {/* Identificação Explícita de Dados e Qualidade */}
      <div className="flex items-center space-x-3">
        <div
          className="flex items-center space-x-1.5 px-2 py-1 bg-amber-950/30 border border-amber-800/60 text-amber-300 font-mono text-[10px]"
          title="Dados de teste e demonstração devidamente identificados como sintéticos baseados em OpenF1."
        >
          <ShieldAlert className="w-3 h-3 text-amber-400" />
          <span>FIXTURE DEMO SINTÉTICA</span>
          <span className="text-amber-500">|</span>
          <span>OpenF1 Sakhir 10Hz</span>
        </div>

        <div className="flex items-center space-x-1 px-2 py-1 bg-[#12161c] border border-[#212836] font-mono text-[10px] text-neutral-400">
          <Cpu className="w-3 h-3 text-sky-400" />
          <span>GRADE ESPACIAL: 5.0m</span>
        </div>

        <div className="h-4 w-px bg-[#212836]" />

        {/* Controles de Layout */}
        <div className="flex items-center space-x-1">
          <button
            onClick={toggleLeftPanel}
            className={`p-1.5 border ${
              leftPanelOpen
                ? 'bg-[#181d26] border-neutral-700 text-neutral-200'
                : 'bg-transparent border-[#212836] text-neutral-500 hover:text-neutral-300'
            }`}
            title="Alternar painel de seleção (Pilotos/Voltas)"
          >
            <PanelLeft className="w-3.5 h-3.5" />
          </button>
          <button
            onClick={toggleBottomPanel}
            className={`p-1.5 border ${
              bottomPanelOpen
                ? 'bg-[#181d26] border-neutral-700 text-neutral-200'
                : 'bg-transparent border-[#212836] text-neutral-500 hover:text-neutral-300'
            }`}
            title="Alternar painel inferior (Direção de Prova/Stints)"
          >
            <PanelBottom className="w-3.5 h-3.5" />
          </button>
          <button
            onClick={toggleRightPanel}
            className={`p-1.5 border ${
              rightPanelOpen
                ? 'bg-[#181d26] border-neutral-700 text-neutral-200'
                : 'bg-transparent border-[#212836] text-neutral-500 hover:text-neutral-300'
            }`}
            title="Alternar painel analítico (Mapa/Insights)"
          >
            <PanelRight className="w-3.5 h-3.5" />
          </button>
        </div>
      </div>
    </header>
  );
};
