'use client';

import React, { useEffect, useState } from 'react';
import {
  Activity,
  Cpu,
  Database,
  Pause,
  Play,
  PanelBottom,
  PanelLeft,
  PanelRight,
  Radio,
  ShieldAlert
} from 'lucide-react';
import { checkGatewayHealth, type GatewayHealth } from '../lib/apiClient';
import { useTelemetryStore } from '../store/telemetryStore';

const REPLAY_SPEEDS = [1, 4, 10, 25];

export const Header: React.FC = () => {
  const {
    leftPanelOpen,
    rightPanelOpen,
    bottomPanelOpen,
    toggleLeftPanel,
    toggleRightPanel,
    toggleBottomPanel,
    liveStreaming,
    liveError,
    replayActive,
    replaySpeed,
    toggleReplay,
    setReplaySpeed,
    sessionYear,
    sessionLabel,
    comparison
  } = useTelemetryStore();

  const [health, setHealth] = useState<GatewayHealth>({ online: false });

  useEffect(() => {
    let mounted = true;
    const controller = new AbortController();

    const probe = async () => {
      const status = await checkGatewayHealth(controller.signal);
      if (mounted) setHealth(status);
    };

    void probe();
    const interval = setInterval(() => void probe(), 10_000);
    return () => {
      mounted = false;
      controller.abort();
      clearInterval(interval);
    };
  }, []);

  const gridStep = comparison.data?.grid_step_m ?? 5.0;

  return (
    <header className="z-30 flex h-12 shrink-0 select-none items-center justify-between border-b border-[#212836] bg-[#0c0e12] px-4">
      <div className="flex items-center space-x-4">
        <div className="flex items-center space-x-2">
          <div className="h-2.5 w-2.5 rotate-45 bg-sky-400" />
          <span className="font-mono text-sm font-bold uppercase tracking-wider text-neutral-100">
            Apex<span className="text-sky-400">Telemetry</span>
          </span>
          {health.version && (
            <span className="border border-[#232936] bg-[#161b24] px-1.5 py-0.5 font-mono text-[10px] text-neutral-400">
              v{health.version}
            </span>
          )}
        </div>

        <div className="h-4 w-px bg-[#212836]" />

        <div className="flex items-center space-x-2 font-mono text-xs text-neutral-400">
          <span className="text-neutral-500">{sessionYear}</span>
          <span>/</span>
          <span className="max-w-[280px] truncate font-medium text-neutral-300">
            {sessionLabel || '—'}
          </span>
        </div>
      </div>

      <div className="flex items-center space-x-3">
        {liveStreaming && (
          <div
            className="flex animate-pulse items-center space-x-1.5 border border-red-700/60 bg-red-950/40 px-2 py-1 font-mono text-[10px] text-red-300"
            title="Recebendo o replay da volta quadro a quadro, direto do gateway, via Server-Sent Events."
          >
            <Radio className="h-3 w-3 text-red-400" />
            <span className="font-bold">REPLAY SSE</span>
          </div>
        )}

        {liveError && !liveStreaming && (
          <div
            className="flex items-center space-x-1.5 border border-amber-800/60 bg-amber-950/30 px-2 py-1 font-mono text-[10px] text-amber-300"
            title={liveError}
          >
            <ShieldAlert className="h-3 w-3 text-amber-400" />
            <span className="max-w-[160px] truncate">{liveError}</span>
          </div>
        )}

        <div className="flex items-center border border-[#212836] bg-[#12161c]">
          <button
            onClick={toggleReplay}
            className={`flex items-center space-x-1.5 px-2 py-1 font-mono text-[10px] transition-colors ${
              replayActive ? 'bg-sky-950/50 text-sky-300' : 'text-neutral-400 hover:text-neutral-200'
            }`}
            title="Reproduzir a volta no tempo real de pista"
          >
            {replayActive ? (
              <Pause className="h-3 w-3 text-sky-400" />
            ) : (
              <Play className="h-3 w-3 text-neutral-400" />
            )}
            <span>REPLAY</span>
          </button>
          <select
            value={replaySpeed}
            onChange={(event) => setReplaySpeed(Number(event.target.value))}
            className="border-l border-[#212836] bg-transparent py-1 pl-1 pr-0.5 font-mono text-[10px] text-neutral-400 outline-none"
            title="Fator de velocidade da reprodução"
          >
            {REPLAY_SPEEDS.map((speed) => (
              <option key={speed} value={speed}>
                {speed}×
              </option>
            ))}
          </select>
        </div>

        {health.online ? (
          <div
            className="flex items-center space-x-1.5 border border-emerald-700/60 bg-emerald-950/40 px-2 py-1 font-mono text-[10px] text-emerald-300"
            title={`Gateway C++23 ativo. Motor numérico: ${health.analyticsEngine}. Upstream: ${health.upstream?.base_url}.`}
          >
            <Activity className="h-3 w-3 animate-pulse text-emerald-400" />
            <span className="font-bold">GATEWAY C++23</span>
            <span className="text-emerald-700">|</span>
            <span>{health.latencyMs}ms</span>
          </div>
        ) : (
          <div
            className="flex items-center space-x-1.5 border border-rose-800/60 bg-rose-950/30 px-2 py-1 font-mono text-[10px] text-rose-300"
            title={health.error ?? 'Gateway inacessível.'}
          >
            <ShieldAlert className="h-3 w-3 text-rose-400" />
            <span className="font-bold">GATEWAY OFFLINE</span>
          </div>
        )}

        {/* Origem real dos dados: armazém ingerido ou consulta ao vivo. */}
        <div
          className="flex items-center space-x-1 border border-[#212836] bg-[#12161c] px-2 py-1 font-mono text-[10px] text-neutral-400"
          title={
            health.database?.healthy
              ? 'Servindo do armazém PostgreSQL/TimescaleDB ingerido.'
              : 'Sem armazém: o gateway consulta a OpenF1 ao vivo e mantém cache em memória.'
          }
        >
          <Database
            className={`h-3 w-3 ${health.database?.healthy ? 'text-emerald-400' : 'text-sky-400'}`}
          />
          <span>{health.database?.healthy ? 'POSTGRES' : 'OPENF1 AO VIVO'}</span>
        </div>

        <div
          className="flex items-center space-x-1 border border-[#212836] bg-[#12161c] px-2 py-1 font-mono text-[10px] text-neutral-400"
          title={
            health.strategyEngine?.healthy
              ? 'Explicações geradas pelo motor de domínio em Haskell.'
              : 'Motor Haskell indisponível: as explicações caem no resumo determinístico em C++.'
          }
        >
          <Cpu
            className={`h-3 w-3 ${health.strategyEngine?.healthy ? 'text-violet-400' : 'text-neutral-600'}`}
          />
          <span>GRADE {gridStep.toFixed(1)}m</span>
        </div>

        <div className="h-4 w-px bg-[#212836]" />

        <div className="flex items-center space-x-1">
          {(
            [
              [leftPanelOpen, toggleLeftPanel, PanelLeft, 'Painel de seleção'],
              [bottomPanelOpen, toggleBottomPanel, PanelBottom, 'Painel inferior'],
              [rightPanelOpen, toggleRightPanel, PanelRight, 'Painel analítico']
            ] as const
          ).map(([open, toggle, Icon, label]) => (
            <button
              key={label}
              onClick={toggle}
              title={label}
              className={`border p-1.5 ${
                open
                  ? 'border-neutral-700 bg-[#181d26] text-neutral-200'
                  : 'border-[#212836] bg-transparent text-neutral-500 hover:text-neutral-300'
              }`}
            >
              <Icon className="h-3.5 w-3.5" />
            </button>
          ))}
        </div>
      </div>
    </header>
  );
};
