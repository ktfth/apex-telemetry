'use client';

import React from 'react';
import { AlertTriangle, CheckCircle2, FileText, Grid, Loader2, ShieldCheck, Zap } from 'lucide-react';
import { useTelemetryStore } from '../store/telemetryStore';
import { CircuitMap } from './CircuitMap';

const EmptyState: React.FC<{ children: React.ReactNode }> = ({ children }) => (
  <div className="px-4 py-8 text-center font-mono text-[11px] leading-relaxed text-neutral-600">
    {children}
  </div>
);

export const InsightsPanel: React.FC = () => {
  const activeInsightId = useTelemetryStore((state) => state.activeInsightId);
  const setActiveInsightId = useTelemetryStore((state) => state.setActiveInsightId);
  const setHoveredDistanceM = useTelemetryStore((state) => state.setHoveredDistanceM);
  const comparison = useTelemetryStore((state) => state.comparison);
  const insightTab = useTelemetryStore((state) => state.insightTab);
  const setInsightTab = useTelemetryStore((state) => state.setInsightTab);

  const data = comparison.data;
  const refCode = data?.reference_lap.driver_code ?? 'REF';
  const compCode = data?.comparison_lap.driver_code ?? 'COMP';

  // Classes completas: o JIT do Tailwind não resolve nomes montados por interpolação.
  const tabs = [
    {
      id: 'insights' as const,
      label: 'INSIGHTS',
      icon: FileText,
      activeClass: 'bg-[#181d26] font-bold text-amber-400 border-b-2 border-b-amber-400'
    },
    {
      id: 'speed_traps' as const,
      label: 'SPEED TRAPS',
      icon: Zap,
      activeClass: 'bg-[#181d26] font-bold text-sky-400 border-b-2 border-b-sky-400'
    },
    {
      id: 'microsectors' as const,
      label: `MICRO (${data?.microsectors.length ?? 0})`,
      icon: Grid,
      activeClass: 'bg-[#181d26] font-bold text-emerald-400 border-b-2 border-b-emerald-400'
    }
  ];

  return (
    <aside className="flex h-full w-96 shrink-0 select-none flex-col overflow-y-auto border-l border-[#212836] bg-[#0d1015]">
      <div className="border-b border-[#212836] p-3">
        <CircuitMap />
      </div>

      <div className="flex border-b border-[#212836] bg-[#11141a] font-mono text-[11px]">
        {tabs.map(({ id, label, icon: Icon, activeClass }) => {
          const active = insightTab === id;
          return (
            <button
              key={id}
              onClick={() => setInsightTab(id)}
              className={`flex flex-1 items-center justify-center space-x-1 border-r border-[#212836] py-2 transition-colors last:border-r-0 ${
                active ? activeClass : 'text-neutral-400 hover:text-neutral-200'
              }`}
            >
              <Icon className="h-3 w-3" />
              <span>{label}</span>
            </button>
          );
        })}
      </div>

      <div className="flex-1 space-y-3 overflow-y-auto p-3">
        {comparison.loading && (
          <div className="flex flex-col items-center gap-2 py-10 font-mono text-[11px] text-neutral-500">
            <Loader2 className="h-4 w-4 animate-spin text-sky-500" />
            <span>Alinhando as duas voltas na grade espacial…</span>
          </div>
        )}

        {!comparison.loading && comparison.error && (
          <div className="flex flex-col items-center gap-2 px-3 py-8 text-center font-mono text-[11px]">
            <AlertTriangle className="h-5 w-5 text-amber-500" />
            <span className="font-bold text-amber-400">{comparison.error.code}</span>
            <span className="leading-relaxed text-neutral-400">{comparison.error.message}</span>
          </div>
        )}

        {!comparison.loading && !comparison.error && !data && (
          <EmptyState>Selecione duas voltas cronometradas para gerar a análise.</EmptyState>
        )}

        {data && insightTab === 'insights' && (
          <>
            {data.insights.length === 0 && (
              <EmptyState>
                Nenhum trecho acumulou perda de tempo acima do limiar de detecção. As duas voltas
                são equivalentes dentro da resolução da medição.
              </EmptyState>
            )}

            {data.insights.map((insight) => {
              const active = insight.id === activeInsightId;
              return (
                <div
                  key={insight.id}
                  onClick={() => {
                    setActiveInsightId(insight.id);
                    setHoveredDistanceM(insight.distance_start_m);
                  }}
                  className={`cursor-pointer border p-3 font-mono text-xs transition-colors ${
                    active
                      ? 'border-amber-600/80 bg-[#181d26] text-neutral-100'
                      : 'border-[#212836] bg-[#11141a] text-neutral-300 hover:bg-[#141922]'
                  }`}
                >
                  <div className="mb-1.5 flex items-center justify-between">
                    <span className="text-[10px] font-bold uppercase tracking-wider text-amber-400">
                      {insight.evidence.corner ?? 'Trecho'} ·{' '}
                      {Math.round(insight.distance_start_m)}–{Math.round(insight.distance_end_m)} m
                    </span>
                    <span className="font-bold tabular-nums text-rose-400">
                      +{insight.time_loss_s.toFixed(3)} s
                    </span>
                  </div>

                  <p className="mb-3 font-sans text-xs leading-relaxed text-neutral-200">
                    {insight.summary}
                  </p>

                  <div className="mb-2 space-y-1 border border-[#1d232e] bg-[#090b0e] p-2">
                    <div className="mb-1 flex items-center justify-between border-b border-[#1a202c] pb-1 text-[9px] font-bold uppercase tracking-wider text-neutral-400">
                      <span>Evidência medida</span>
                      <span className="text-emerald-400">{insight.evidence.cause ?? 'MIXED'}</span>
                    </div>
                    <div className="flex justify-between text-[10px]">
                      <span className="text-neutral-400">V. mínima ({refCode} / {compCode}):</span>
                      <span className="tabular-nums text-neutral-200">
                        {insight.evidence.min_speed_ref_kmh.toFixed(1)} /{' '}
                        {insight.evidence.min_speed_comp_kmh.toFixed(1)} km/h
                      </span>
                    </div>
                    <div className="flex justify-between text-[10px]">
                      <span className="text-neutral-400">95% acelerador:</span>
                      <span className="tabular-nums text-neutral-200">
                        {Math.round(insight.evidence.full_throttle_distance_ref_m)} /{' '}
                        {Math.round(insight.evidence.full_throttle_distance_comp_m)} m
                      </span>
                    </div>
                    <div className="flex justify-between text-[10px]">
                      <span className="text-neutral-400">Ponto de freada:</span>
                      <span className="tabular-nums text-neutral-200">
                        {insight.evidence.braking_point_diff_m > 0 ? '+' : ''}
                        {insight.evidence.braking_point_diff_m.toFixed(1)} m
                      </span>
                    </div>
                  </div>

                  <div className="space-y-1 border-t border-[#1a202c] pt-2 text-[10px] text-neutral-400">
                    {insight.assumptions.map((assumption) => (
                      <div key={assumption} className="flex items-start space-x-1">
                        <CheckCircle2 className="mt-0.5 h-3 w-3 shrink-0 text-emerald-400" />
                        <span className="leading-relaxed">{assumption}</span>
                      </div>
                    ))}
                    {insight.limitations.map((limitation) => (
                      <div key={limitation} className="flex items-start space-x-1">
                        <AlertTriangle className="mt-0.5 h-3 w-3 shrink-0 text-amber-400" />
                        <span className="leading-relaxed">{limitation}</span>
                      </div>
                    ))}
                  </div>

                  <div className="mt-2 flex items-center justify-between pt-1 text-[10px] text-neutral-400">
                    <span>{insight.engine ?? 'analytics-cpp'}</span>
                    <span>
                      confiança{' '}
                      <span className="font-bold tabular-nums text-sky-300">
                        {(insight.confidence * 100).toFixed(0)}%
                      </span>
                    </span>
                  </div>
                </div>
              );
            })}
          </>
        )}

        {data && insightTab === 'speed_traps' && (
          <div className="space-y-2">
            <div className="mb-1 font-mono text-[10px] uppercase tracking-wider text-neutral-400">
              Sensores oficiais e picos medidos
            </div>
            {data.speed_traps.length === 0 && (
              <EmptyState>Nenhum ponto de medição identificado nesta volta.</EmptyState>
            )}
            {data.speed_traps.map((trap) => (
              <div
                key={`${trap.name}-${trap.distance_m}`}
                onClick={() => setHoveredDistanceM(trap.distance_m)}
                className="cursor-pointer border border-[#212836] bg-[#11141a] p-2.5 font-mono text-xs transition-colors hover:border-sky-600/60"
              >
                <div className="mb-1 flex items-center justify-between">
                  <span className="text-[11px] font-bold text-neutral-200">{trap.name}</span>
                  <span className="text-[10px] text-neutral-500">
                    {Math.round(trap.distance_m)} m
                  </span>
                </div>
                <div className="flex items-baseline justify-between text-[10px]">
                  <span className="text-neutral-400">
                    {refCode} / {compCode}
                  </span>
                  <span className="tabular-nums text-neutral-200">
                    {trap.ref_speed_kmh.toFixed(1)} / {trap.comp_speed_kmh.toFixed(1)} km/h
                  </span>
                  <span
                    className={`font-bold tabular-nums ${trap.delta_kmh >= 0 ? 'text-emerald-400' : 'text-rose-400'}`}
                  >
                    {trap.delta_kmh >= 0 ? '+' : ''}
                    {trap.delta_kmh.toFixed(1)}
                  </span>
                </div>
                <div className="mt-1 text-[9px] text-neutral-600">
                  {trap.origin === 'openf1_marshalling_loop'
                    ? 'sensor oficial de passagem'
                    : 'pico medido no canal de velocidade'}
                </div>
              </div>
            ))}
          </div>
        )}

        {data && insightTab === 'microsectors' && (
          <div className="space-y-2">
            <div className="mb-1 flex justify-between font-mono text-[10px] uppercase tracking-wider text-neutral-400">
              <span>Microsetores de 100 m</span>
              <span className="text-emerald-400">{data.microsectors.length} setores</span>
            </div>
            <div className="grid grid-cols-6 gap-1">
              {data.microsectors.map((microsector) => (
                <button
                  key={microsector.index}
                  onClick={() => setHoveredDistanceM(microsector.distance_start_m)}
                  title={`${Math.round(microsector.distance_start_m)}–${Math.round(microsector.distance_end_m)} m · delta ${microsector.delta_s >= 0 ? '+' : ''}${microsector.delta_s.toFixed(3)} s`}
                  className={`flex h-6 items-center justify-center border font-mono text-[9px] font-bold transition-colors ${
                    microsector.winner === 'REF'
                      ? 'border-sky-600/70 bg-sky-950/70 text-sky-300'
                      : microsector.winner === 'COMP'
                        ? 'border-rose-600/70 bg-rose-950/70 text-rose-300'
                        : 'border-neutral-700 bg-neutral-800 text-neutral-400'
                  }`}
                >
                  {microsector.index + 1}
                </button>
              ))}
            </div>
            <div className="flex justify-between border-t border-[#1a202c] pt-2 font-mono text-[10px] text-neutral-400">
              <span className="flex items-center space-x-1">
                <span className="inline-block h-2 w-2 bg-sky-500" />
                <span>{refCode} mais rápido</span>
              </span>
              <span className="flex items-center space-x-1">
                <span className="inline-block h-2 w-2 bg-rose-500" />
                <span>{compCode} mais rápido</span>
              </span>
            </div>
          </div>
        )}
      </div>

      {data && (
        <div className="border-t border-[#212836] bg-[#0c0e12] p-3 font-mono text-[10px] text-neutral-400">
          <div className="mb-1 flex items-center space-x-1.5 font-semibold text-neutral-300">
            <ShieldCheck className="h-3.5 w-3.5 text-emerald-400" />
            <span>AUDITORIA DE QUALIDADE</span>
          </div>
          <div className="flex justify-between">
            <span>Fechamento do delta vs cronômetro:</span>
            <span
              className={`font-bold tabular-nums ${
                data.quality_audit.delta_closure_error_s <= 0.02 ? 'text-emerald-400' : 'text-amber-400'
              }`}
            >
              {data.quality_audit.delta_closure_error_s.toFixed(4)} s
            </span>
          </div>
          <div className="flex justify-between">
            <span>Cobertura da grade:</span>
            <span className="tabular-nums text-neutral-200">
              {data.quality_audit.coverage_pct.toFixed(1)}%
            </span>
          </div>
          <div className="flex justify-between">
            <span>Maior lacuna entre amostras:</span>
            <span className="tabular-nums text-neutral-200">
              {data.quality_audit.max_interpolation_gap_m.toFixed(1)} m
            </span>
          </div>
          <div className="flex justify-between">
            <span>Intervalo mediano de amostragem:</span>
            <span className="tabular-nums text-neutral-200">
              {data.quality_audit.median_sample_interval_s.toFixed(3)} s
            </span>
          </div>
          <div className="flex justify-between">
            <span>Escore de integridade:</span>
            <span className="font-bold tabular-nums text-emerald-400">
              {(data.quality_audit.confidence_score * 100).toFixed(0)}%
            </span>
          </div>
          <p className="mt-1.5 border-t border-[#1a202c] pt-1.5 text-[9px] leading-relaxed text-neutral-600">
            {data.quality_audit.source_notes}
          </p>
        </div>
      )}
    </aside>
  );
};
