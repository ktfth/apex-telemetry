'use client';

import React, { useState } from 'react';
import { useTelemetryStore } from '../store/telemetryStore';
import { CircuitMap } from './CircuitMap';
import { FileText, CheckCircle2, AlertTriangle, ShieldCheck, Zap, Grid } from 'lucide-react';

export const InsightsPanel: React.FC = () => {
  const { activeInsightId, setActiveInsightId, setHoveredDistanceM, comparison } = useTelemetryStore();
  const [tab, setTab] = useState<'insights' | 'speed_traps' | 'microsectors'>('insights');

  const insights = comparison.insights || [];
  const audit = comparison.quality_audit;
  const speedTraps = comparison.speed_traps || [];
  const microsectors = comparison.microsectors || [];

  return (
    <aside className="w-96 bg-[#0d1015] border-l border-[#212836] flex flex-col h-full select-none shrink-0 overflow-y-auto">
      {/* Mapa do Circuito integrado no topo do painel direito */}
      <div className="p-3 border-b border-[#212836]">
        <CircuitMap />
      </div>

      {/* Seletor de Abas Analíticas */}
      <div className="flex border-b border-[#212836] bg-[#11141a] text-[11px] font-mono">
        <button
          onClick={() => setTab('insights')}
          className={`flex-1 py-2 flex items-center justify-center space-x-1 border-r border-[#212836] transition-colors ${
            tab === 'insights'
              ? 'bg-[#181d26] text-amber-400 font-bold border-b-2 border-b-amber-400'
              : 'text-neutral-400 hover:text-neutral-200'
          }`}
        >
          <FileText className="w-3 h-3" />
          <span>INSIGHTS</span>
        </button>
        <button
          onClick={() => setTab('speed_traps')}
          className={`flex-1 py-2 flex items-center justify-center space-x-1 border-r border-[#212836] transition-colors ${
            tab === 'speed_traps'
              ? 'bg-[#181d26] text-sky-400 font-bold border-b-2 border-b-sky-400'
              : 'text-neutral-400 hover:text-neutral-200'
          }`}
        >
          <Zap className="w-3 h-3" />
          <span>SPEED TRAPS</span>
        </button>
        <button
          onClick={() => setTab('microsectors')}
          className={`flex-1 py-2 flex items-center justify-center space-x-1 transition-colors ${
            tab === 'microsectors'
              ? 'bg-[#181d26] text-emerald-400 font-bold border-b-2 border-b-emerald-400'
              : 'text-neutral-400 hover:text-neutral-200'
          }`}
        >
          <Grid className="w-3 h-3" />
          <span>MICRO (55)</span>
        </button>
      </div>

      {/* Conteúdo da Aba Ativa */}
      <div className="p-3 space-y-3 flex-1 overflow-y-auto">
        {tab === 'insights' && (
          <>
            {insights.map((ins) => {
              const isActive = ins.id === activeInsightId;
              return (
                <div
                  key={ins.id}
                  onClick={() => {
                    setActiveInsightId(ins.id);
                    setHoveredDistanceM(ins.distance_start_m);
                  }}
                  className={`p-3 border cursor-pointer transition-colors font-mono text-xs ${
                    isActive
                      ? 'bg-[#181d26] border-amber-600/80 text-neutral-100 shadow-sm'
                      : 'bg-[#11141a] border-[#212836] text-neutral-300 hover:bg-[#141922]'
                  }`}
                >
                  <div className="flex items-center justify-between mb-1.5">
                    <span className="text-[10px] font-bold text-amber-400 uppercase tracking-wider">
                      TRECHO {ins.distance_start_m}m — {ins.distance_end_m}m
                    </span>
                    <span className="text-rose-400 font-bold tabular-nums">
                      +{ins.time_loss_s.toFixed(3)}s
                    </span>
                  </div>

                  <p className="text-xs text-neutral-200 font-sans leading-relaxed mb-3">
                    {ins.summary}
                  </p>

                  <div className="bg-[#090b0e] border border-[#1d232e] p-2 space-y-1 mb-2">
                    <div className="text-[9px] font-bold text-neutral-400 uppercase tracking-wider flex items-center justify-between border-b border-[#1a202c] pb-1 mb-1">
                      <span>Evidência Empírica Observada</span>
                      <span className="text-emerald-400">AUDITADO</span>
                    </div>
                    <div className="flex justify-between text-[10px]">
                      <span className="text-neutral-400">V. Mínima (Ref vs Comp):</span>
                      <span className="tabular-nums text-neutral-200">
                        {ins.evidence.min_speed_ref_kmh} vs {ins.evidence.min_speed_comp_kmh} km/h
                      </span>
                    </div>
                    <div className="flex justify-between text-[10px]">
                      <span className="text-neutral-400">Distância 95% Throttle:</span>
                      <span className="tabular-nums text-neutral-200">
                        {ins.evidence.full_throttle_distance_ref_m}m vs {ins.evidence.full_throttle_distance_comp_m}m
                      </span>
                    </div>
                    <div className="flex justify-between text-[10px]">
                      <span className="text-neutral-400">Ponto de Frenagem:</span>
                      <span className="tabular-nums text-neutral-200">
                        {ins.evidence.braking_point_diff_m > 0 ? '+' : ''}{ins.evidence.braking_point_diff_m}m
                      </span>
                    </div>
                  </div>

                  <div className="space-y-1 text-[10px] text-neutral-400 border-t border-[#1a202c] pt-2">
                    <div className="flex items-center space-x-1 text-neutral-400">
                      <CheckCircle2 className="w-3 h-3 text-emerald-400 shrink-0" />
                      <span className="truncate">{ins.assumptions[0]}</span>
                    </div>
                    <div className="flex items-center space-x-1 text-neutral-400">
                      <AlertTriangle className="w-3 h-3 text-amber-400 shrink-0" />
                      <span className="truncate">{ins.limitations[0]}</span>
                    </div>
                  </div>

                  <div className="mt-2 flex items-center justify-between text-[10px] text-neutral-400 pt-1">
                    <span>Confiança Formal:</span>
                    <span className="text-sky-300 font-bold tabular-nums">
                      {(ins.confidence * 100).toFixed(0)}%
                    </span>
                  </div>
                </div>
              );
            })}
          </>
        )}

        {tab === 'speed_traps' && (
          <div className="space-y-2">
            <div className="text-[10px] font-mono text-neutral-400 uppercase tracking-wider mb-1">
              Pontos Oficiais de Medição de Velocidade
            </div>
            {speedTraps.map((st, i) => (
              <div
                key={i}
                onClick={() => setHoveredDistanceM(st.distance_m)}
                className="p-2.5 bg-[#11141a] border border-[#212836] hover:border-sky-600/60 cursor-pointer font-mono text-xs transition-colors"
              >
                <div className="flex justify-between items-center mb-1">
                  <span className="text-neutral-200 font-bold text-[11px]">{st.name}</span>
                  <span className="text-[10px] text-neutral-500">{st.distance_m}m</span>
                </div>
                <div className="flex justify-between items-baseline text-[10px]">
                  <span className="text-neutral-400">VER vs LEC:</span>
                  <span className="tabular-nums text-neutral-200">
                    {st.ref_speed_kmh.toFixed(1)} vs {st.comp_speed_kmh.toFixed(1)} km/h
                  </span>
                  <span className={`font-bold tabular-nums ${st.delta_kmh >= 0 ? 'text-emerald-400' : 'text-rose-400'}`}>
                    {st.delta_kmh >= 0 ? '+' : ''}{st.delta_kmh.toFixed(1)} km/h
                  </span>
                </div>
              </div>
            ))}
          </div>
        )}

        {tab === 'microsectors' && (
          <div className="space-y-2">
            <div className="text-[10px] font-mono text-neutral-400 uppercase tracking-wider mb-1 flex justify-between">
              <span>Grade de Microsetores (100m)</span>
              <span className="text-emerald-400">54 Mini-Sectors</span>
            </div>
            <div className="grid grid-cols-6 gap-1">
              {microsectors.map((m) => (
                <button
                  key={m.index}
                  onClick={() => setHoveredDistanceM(m.distance_start_m)}
                  title={`Mini-Sector ${m.index + 1} (${m.distance_start_m}m - ${m.distance_end_m}m): Delta ${m.delta_s >= 0 ? '+' : ''}${m.delta_s.toFixed(3)}s [Winner: ${m.winner}]`}
                  className={`h-6 text-[9px] font-mono font-bold flex items-center justify-center border transition-colors ${
                    m.winner === 'REF'
                      ? 'bg-sky-950/70 border-sky-600/70 text-sky-300'
                      : m.winner === 'COMP'
                      ? 'bg-rose-950/70 border-rose-600/70 text-rose-300'
                      : 'bg-neutral-800 border-neutral-700 text-neutral-400'
                  }`}
                >
                  {m.index + 1}
                </button>
              ))}
            </div>
            <div className="flex justify-between text-[10px] font-mono text-neutral-400 pt-2 border-t border-[#1a202c]">
              <span className="flex items-center space-x-1">
                <span className="w-2 h-2 bg-sky-500 inline-block" />
                <span>VER mais rápido</span>
              </span>
              <span className="flex items-center space-x-1">
                <span className="w-2 h-2 bg-rose-500 inline-block" />
                <span>LEC mais rápido</span>
              </span>
            </div>
          </div>
        )}
      </div>

      {/* Auditoria de Qualidade de Dados */}
      <div className="p-3 border-t border-[#212836] bg-[#0c0e12] font-mono text-[10px] text-neutral-400">
        <div className="flex items-center space-x-1.5 text-neutral-300 font-semibold mb-1">
          <ShieldCheck className="w-3.5 h-3.5 text-emerald-400" />
          <span>AUDITORIA DE QUALIDADE DOS DADOS</span>
        </div>
        <div className="flex justify-between">
          <span>Gap Máx. Interpolação:</span>
          <span className="text-neutral-200 tabular-nums">{audit.max_interpolation_gap_m}m</span>
        </div>
        <div className="flex justify-between">
          <span>Segmentos Descontínuos:</span>
          <span className="text-emerald-400 tabular-nums">{audit.discontinuous_segments}</span>
        </div>
        <div className="flex justify-between">
          <span>Escore de Integridade:</span>
          <span className="text-emerald-400 font-bold tabular-nums">{(audit.confidence_score * 100).toFixed(0)}%</span>
        </div>
      </div>
    </aside>
  );
};
