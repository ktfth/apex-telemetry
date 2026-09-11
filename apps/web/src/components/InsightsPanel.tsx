'use client';

import React from 'react';
import { useTelemetryStore } from '../store/telemetryStore';
import { CircuitMap } from './CircuitMap';
import { FileText, CheckCircle2, AlertTriangle, HelpCircle, ShieldCheck } from 'lucide-react';

export const InsightsPanel: React.FC = () => {
  const { activeInsightId, setActiveInsightId, setHoveredDistanceM, comparison } = useTelemetryStore();
  const insights = comparison.insights;
  const audit = comparison.quality_audit;

  return (
    <aside className="w-96 bg-[#0d1015] border-l border-[#212836] flex flex-col h-full select-none shrink-0 overflow-y-auto">
      {/* Mapa do Circuito integrado no topo do painel direito */}
      <div className="p-3 border-b border-[#212836]">
        <CircuitMap />
      </div>

      {/* Cabeçalho de Insights */}
      <div className="p-3 border-b border-[#212836] bg-[#11141a] flex items-center justify-between">
        <div className="flex items-center space-x-1.5 text-xs font-mono font-semibold text-neutral-200 uppercase tracking-wider">
          <FileText className="w-3.5 h-3.5 text-amber-400" />
          <span>Insights Explicáveis & Evidências</span>
        </div>
        <span className="text-[10px] font-mono px-1.5 py-0.5 bg-neutral-800 text-neutral-300">
          HASKELL PURE
        </span>
      </div>

      {/* Lista de Insights com Evidência Numérica */}
      <div className="p-3 space-y-3 flex-1">
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
              {/* Top metadata do insight */}
              <div className="flex items-center justify-between mb-1.5">
                <span className="text-[10px] font-bold text-amber-400 uppercase tracking-wider">
                  TRECHO {ins.distance_start_m}m — {ins.distance_end_m}m
                </span>
                <span className="text-rose-400 font-bold tabular-nums">
                  +{ins.time_loss_s.toFixed(3)}s
                </span>
              </div>

              {/* Texto explicável rigoroso */}
              <p className="text-xs text-neutral-200 font-sans leading-relaxed mb-3">
                {ins.summary}
              </p>

              {/* Tabela de Evidências Empíricas */}
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

              {/* Premissas e Limitações */}
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

              {/* Confiança Formal */}
              <div className="mt-2 flex items-center justify-between text-[10px] text-neutral-400 pt-1">
                <span>Confiança Formal:</span>
                <span className="text-sky-300 font-bold tabular-nums">
                  {(ins.confidence * 100).toFixed(0)}%
                </span>
              </div>
            </div>
          );
        })}
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
