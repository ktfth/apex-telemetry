'use client';

import React, { useEffect, useState } from 'react';
import { useTelemetryStore } from '../store/telemetryStore';
import { checkApiHealth, ApiStatus } from '../lib/apiClient';
import { PanelLeft, PanelRight, PanelBottom, ShieldAlert, Cpu, Activity, Radio, Play, Pause } from 'lucide-react';

export const Header: React.FC = () => {
  const {
    leftPanelOpen,
    rightPanelOpen,
    bottomPanelOpen,
    toggleLeftPanel,
    toggleRightPanel,
    toggleBottomPanel,
    liveStreaming,
    replayActive,
    toggleReplay
  } = useTelemetryStore();

  const [apiStatus, setApiStatus] = useState<ApiStatus>({ online: false });

  useEffect(() => {
    let mounted = true;
    const probe = async () => {
      const status = await checkApiHealth();
      if (mounted) {
        setApiStatus(status);
      }
    };
    probe();
    const interval = setInterval(probe, 8000);
    return () => {
      mounted = false;
      clearInterval(interval);
    };
  }, []);

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
            v1.6.0-fase5
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

      {/* Indicadores de Origem, Backend C++, SSE Live e Replay */}
      <div className="flex items-center space-x-3">
        {/* Live SSE Stream Badge */}
        {liveStreaming && (
          <div
            className="flex items-center space-x-1.5 px-2 py-1 bg-red-950/40 border border-red-700/60 text-red-300 font-mono text-[10px] animate-pulse"
            title="Recebendo stream de telemetria e controle de corrida em tempo real via Server-Sent Events (SSE)"
          >
            <Radio className="w-3 h-3 text-red-400" />
            <span className="font-bold">LIVE SSE</span>
          </div>
        )}

        {/* Replay Mode Toggle */}
        <button
          onClick={toggleReplay}
          className={`flex items-center space-x-1.5 px-2 py-1 border font-mono text-[10px] transition-colors ${
            replayActive
              ? 'bg-sky-950/50 border-sky-600 text-sky-300'
              : 'bg-[#12161c] border-[#212836] text-neutral-400 hover:text-neutral-200'
          }`}
          title="Alternar modo de reprodução e replay de telemetria"
        >
          {replayActive ? <Pause className="w-3 h-3 text-sky-400" /> : <Play className="w-3 h-3 text-neutral-400" />}
          <span>{replayActive ? 'REPLAY [PAUSE]' : 'REPLAY [PLAY]'}</span>
        </button>

        {apiStatus.online ? (
          <div
            className="flex items-center space-x-1.5 px-2 py-1 bg-emerald-950/40 border border-emerald-700/60 text-emerald-300 font-mono text-[10px]"
            title="Conectado ao api-gateway-cpp com motor numérico analytics-cpp ativo em C++23."
          >
            <Activity className="w-3 h-3 text-emerald-400 animate-pulse" />
            <span className="font-bold">API GATEWAY C++23 [ONLINE]</span>
            <span className="text-emerald-600">|</span>
            <span>analytics-cpp (5m)</span>
            <span className="text-emerald-600">|</span>
            <span>{apiStatus.latencyMs}ms</span>
          </div>
        ) : (
          <div
            className="flex items-center space-x-1.5 px-2 py-1 bg-amber-950/30 border border-amber-800/60 text-amber-300 font-mono text-[10px]"
            title="API Gateway local offline. Operando em modo de demonstração com fixture auditada do GP do Bahrein 2024."
          >
            <ShieldAlert className="w-3 h-3 text-amber-400" />
            <span>STANDALONE DEMO</span>
            <span className="text-amber-500">|</span>
            <span>OpenF1 Sakhir 10Hz</span>
          </div>
        )}

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
