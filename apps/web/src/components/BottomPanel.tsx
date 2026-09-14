'use client';

import React, { useEffect, useState } from 'react';
import { AlertTriangle, Disc, Loader2, Radio, RefreshCcw, Timer } from 'lucide-react';
import { TyreBadge } from '@apex-telemetry/ui';
import type { DegradationReport, Lap, RaceControlEvent } from '@apex-telemetry/contracts';
import { ApiClientError, fetchDegradation, fetchLaps, fetchRaceControl } from '../lib/apiClient';
import { useTelemetryStore } from '../store/telemetryStore';

interface Remote<T> {
  data: T | null;
  loading: boolean;
  error: ApiClientError | null;
}

const initial = <T,>(): Remote<T> => ({ data: null, loading: false, error: null });

function toApiError(error: unknown): ApiClientError {
  return error instanceof ApiClientError
    ? error
    : new ApiClientError(String(error), 'UNEXPECTED_ERROR', 0);
}

const flagClass = (flag: string): string => {
  switch (flag.toUpperCase()) {
    case 'YELLOW':
    case 'DOUBLE YELLOW':
      return 'border-amber-600 bg-amber-950/40 text-amber-300';
    case 'GREEN':
    case 'CLEAR':
      return 'border-emerald-600 bg-emerald-950/40 text-emerald-300';
    case 'RED':
      return 'border-red-600 bg-red-950/40 text-red-300';
    case 'CHEQUERED':
      return 'border-neutral-400 bg-neutral-700/40 text-neutral-100';
    case 'BLUE':
      return 'border-sky-600 bg-sky-950/40 text-sky-300';
    default:
      return 'border-neutral-700 bg-neutral-800 text-neutral-300';
  }
};

const ErrorRow: React.FC<{ error: ApiClientError }> = ({ error }) => (
  <div className="flex items-center justify-center gap-2 py-8 text-center font-mono text-[11px] text-neutral-500">
    <AlertTriangle className="h-4 w-4 text-amber-500" />
    <span>
      <span className="font-bold text-amber-400">{error.code}</span> — {error.message}
    </span>
  </div>
);

export const BottomPanel: React.FC = () => {
  const sessionKey = useTelemetryStore((state) => state.sessionKey);
  const refDriverNumber = useTelemetryStore((state) => state.refDriverNumber);
  const refDriverCode = useTelemetryStore((state) => state.refDriverCode);
  const compDriverNumber = useTelemetryStore((state) => state.compDriverNumber);
  const compDriverCode = useTelemetryStore((state) => state.compDriverCode);
  const bottomTab = useTelemetryStore((state) => state.bottomTab);
  const setBottomTab = useTelemetryStore((state) => state.setBottomTab);

  const [raceControl, setRaceControl] = useState<Remote<RaceControlEvent[]>>(initial);
  const [laps, setLaps] = useState<Remote<{ ref: Lap[]; comp: Lap[] }>>(initial);
  const [degradation, setDegradation] = useState<Remote<DegradationReport[]>>(initial);

  useEffect(() => {
    if (sessionKey === null) {
      setRaceControl(initial());
      return;
    }
    const controller = new AbortController();
    setRaceControl({ data: null, loading: true, error: null });
    fetchRaceControl(sessionKey, controller.signal)
      .then((result) => {
        if (!controller.signal.aborted) {
          setRaceControl({ data: result.data, loading: false, error: null });
        }
      })
      .catch((error) => {
        if (!controller.signal.aborted) {
          setRaceControl({ data: null, loading: false, error: toApiError(error) });
        }
      });
    return () => controller.abort();
  }, [sessionKey]);

  useEffect(() => {
    if (sessionKey === null || refDriverNumber === null || compDriverNumber === null) {
      setLaps(initial());
      return;
    }
    const controller = new AbortController();
    setLaps({ data: null, loading: true, error: null });
    Promise.all([
      fetchLaps(sessionKey, refDriverNumber, controller.signal),
      fetchLaps(sessionKey, compDriverNumber, controller.signal)
    ])
      .then(([ref, comp]) => {
        if (!controller.signal.aborted) {
          setLaps({ data: { ref: ref.data, comp: comp.data }, loading: false, error: null });
        }
      })
      .catch((error) => {
        if (!controller.signal.aborted) {
          setLaps({ data: null, loading: false, error: toApiError(error) });
        }
      });
    return () => controller.abort();
  }, [sessionKey, refDriverNumber, compDriverNumber]);

  // A degradação é cara (atravessa o motor Haskell): só é buscada na aba ativa.
  useEffect(() => {
    if (bottomTab !== 'stints') return;
    if (sessionKey === null || refDriverNumber === null || compDriverNumber === null) {
      setDegradation(initial());
      return;
    }
    const controller = new AbortController();
    setDegradation({ data: null, loading: true, error: null });
    Promise.all([
      fetchDegradation(sessionKey, refDriverNumber, controller.signal),
      fetchDegradation(sessionKey, compDriverNumber, controller.signal)
    ])
      .then(([ref, comp]) => {
        if (!controller.signal.aborted) {
          setDegradation({ data: [ref.data, comp.data], loading: false, error: null });
        }
      })
      .catch((error) => {
        if (!controller.signal.aborted) {
          setDegradation({ data: null, loading: false, error: toApiError(error) });
        }
      });
    return () => controller.abort();
  }, [bottomTab, sessionKey, refDriverNumber, compDriverNumber]);

  const tabs = [
    { id: 'race_control' as const, label: 'DIREÇÃO DE PROVA', icon: Radio, busy: raceControl.loading },
    { id: 'stints' as const, label: 'STINTS & DEGRADAÇÃO', icon: Disc, busy: degradation.loading },
    { id: 'laps' as const, label: 'MATRIZ DE VOLTAS', icon: Timer, busy: laps.loading }
  ];

  const renderLapColumn = (title: string, accent: string, rows: Lap[]) => (
    <div>
      <div className={`mb-1 text-[10px] font-bold uppercase ${accent}`}>{title}</div>
      <div className="max-h-36 space-y-1 overflow-y-auto">
        {rows.length === 0 && (
          <div className="py-4 text-center text-[11px] text-neutral-600">Sem voltas registradas.</div>
        )}
        {rows.map((lap) => (
          <div
            key={lap.lap_number}
            className="flex items-center justify-between border border-[#1f2532] bg-[#12161c] p-1.5"
          >
            <span>Volta {lap.lap_number}</span>
            <span className="font-bold tabular-nums text-neutral-200">
              {lap.lap_time_s === null ? '—' : `${lap.lap_time_s.toFixed(3)} s`}
            </span>
            <span className="text-[10px] text-neutral-500">
              {lap.coverage_pct === null ? 'cob. n/d' : `cob. ${lap.coverage_pct.toFixed(0)}%`}
            </span>
          </div>
        ))}
      </div>
    </div>
  );

  return (
    <div className="flex h-52 shrink-0 select-none flex-col overflow-hidden border-t border-[#212836] bg-[#0c0e12]">
      <div className="flex items-center space-x-1 border-b border-[#212836] bg-[#090b0e] px-3 pt-1">
        {tabs.map(({ id, label, icon: Icon, busy }) => (
          <button
            key={id}
            onClick={() => setBottomTab(id)}
            className={`flex items-center space-x-1.5 border-t-2 px-3 py-1.5 font-mono text-xs font-medium transition-colors ${
              bottomTab === id
                ? 'border-sky-500 bg-[#12161c] text-neutral-100'
                : 'border-transparent text-neutral-400 hover:text-neutral-200'
            }`}
          >
            <Icon className="h-3 w-3 text-amber-400" />
            <span>{label}</span>
            {busy && <RefreshCcw className="ml-1 h-2.5 w-2.5 animate-spin text-neutral-500" />}
          </button>
        ))}
      </div>

      <div className="flex-1 overflow-y-auto p-3 font-mono text-xs">
        {bottomTab === 'race_control' && (
          <>
            {raceControl.error && <ErrorRow error={raceControl.error} />}
            {raceControl.loading && (
              <div className="flex justify-center py-8">
                <Loader2 className="h-4 w-4 animate-spin text-sky-500" />
              </div>
            )}
            {raceControl.data && raceControl.data.length === 0 && (
              <div className="py-6 text-center text-neutral-500">
                Nenhuma mensagem de direção de prova registrada nesta sessão.
              </div>
            )}
            <div className="space-y-1.5">
              {raceControl.data?.map((event, index) => (
                <div
                  key={`${event.occurred_at}-${index}`}
                  className="flex items-center justify-between border border-[#1d232e] bg-[#12161c] p-2"
                >
                  <div className="flex min-w-0 items-center space-x-3">
                    <span className="shrink-0 text-[10px] tabular-nums text-neutral-500">
                      {event.occurred_at.slice(11, 19)} UTC
                    </span>
                    <span
                      className={`shrink-0 border px-1.5 py-0.5 text-[9px] font-bold ${flagClass(event.flag || event.category)}`}
                    >
                      {event.flag || event.category || 'INFO'}
                    </span>
                    <span className="truncate font-sans text-xs font-medium text-neutral-200">
                      {event.message}
                    </span>
                  </div>
                  <div className="ml-2 flex shrink-0 items-center gap-1.5">
                    {event.driver_number !== null && (
                      <span className="bg-[#1b212c] px-1.5 py-0.5 text-[10px] text-neutral-400">
                        #{event.driver_number}
                      </span>
                    )}
                    {event.sector !== null && (
                      <span className="bg-[#1b212c] px-1.5 py-0.5 text-[10px] text-neutral-400">
                        SETOR {event.sector}
                      </span>
                    )}
                  </div>
                </div>
              ))}
            </div>
          </>
        )}

        {bottomTab === 'stints' && (
          <>
            {degradation.error && <ErrorRow error={degradation.error} />}
            {degradation.loading && (
              <div className="flex items-center justify-center gap-2 py-8 text-[11px] text-neutral-500">
                <Loader2 className="h-4 w-4 animate-spin text-sky-500" />
                <span>Regredindo os tempos reais de cada stint no motor de domínio…</span>
              </div>
            )}
            {degradation.data && (
              <div className="overflow-x-auto">
                <table className="w-full border-collapse text-left">
                  <thead>
                    <tr className="border-b border-[#212836] text-[10px] uppercase tracking-wider text-neutral-400">
                      <th className="px-2 py-1">Piloto</th>
                      <th className="px-2 py-1">Stint</th>
                      <th className="px-2 py-1">Composto</th>
                      <th className="px-2 py-1">Voltas</th>
                      <th className="px-2 py-1" title="Média das voltas representativas">
                        Média
                      </th>
                      <th className="px-2 py-1" title="Inclinação medida na regressão dos tempos reais">
                        Degradação medida
                      </th>
                      <th className="px-2 py-1" title="Perda acumulada prevista pelo modelo térmico">
                        Prevista (modelo)
                      </th>
                      <th className="px-2 py-1">Recomendação</th>
                    </tr>
                  </thead>
                  <tbody className="divide-y divide-[#1b212c]">
                    {degradation.data.flatMap((report, reportIndex) =>
                      report.stints.map((stint) => {
                        const code = reportIndex === 0 ? refDriverCode : compDriverCode;
                        const number = reportIndex === 0 ? refDriverNumber : compDriverNumber;
                        return (
                          <tr
                            key={`${report.session_key}-${stint.driver_number}-${stint.stint_number}`}
                            className="hover:bg-[#151a22]"
                          >
                            <td className="px-2 py-1 font-bold text-neutral-200">
                              {code || `#${number}`}
                            </td>
                            <td className="px-2 py-1">{stint.stint_number}</td>
                            <td className="px-2 py-1">
                              <TyreBadge compound={stint.compound} />
                            </td>
                            <td className="px-2 py-1 tabular-nums">
                              L{stint.lap_start}→L{stint.lap_end}
                              <span className="ml-1 text-neutral-500">
                                ({stint.representative_laps}/{stint.lap_count} úteis)
                              </span>
                            </td>
                            <td className="px-2 py-1 tabular-nums">
                              {stint.avg_lap_time_s > 0 ? `${stint.avg_lap_time_s.toFixed(2)} s` : '—'}
                            </td>
                            <td
                              className={`px-2 py-1 tabular-nums ${
                                stint.representative_laps < 3
                                  ? 'text-neutral-600'
                                  : stint.observed_degradation_s_per_lap > 0
                                    ? 'text-amber-400'
                                    : 'text-emerald-400'
                              }`}
                              title={stint.notes.join(' ')}
                            >
                              {stint.representative_laps < 3
                                ? 'insuficiente'
                                : `${stint.observed_degradation_s_per_lap >= 0 ? '+' : ''}${stint.observed_degradation_s_per_lap.toFixed(3)} s/volta`}
                            </td>
                            <td className="px-2 py-1 tabular-nums text-neutral-400">
                              {stint.predicted_pace_loss_s.toFixed(2)} s
                              {stint.in_cliff && (
                                <span className="ml-1 text-rose-400" title="Além do cliff nominal">
                                  ▲
                                </span>
                              )}
                            </td>
                            <td className="max-w-[280px] px-2 py-1 text-[10px] text-neutral-400">
                              {stint.recommendation ? (
                                <span title={stint.recommendation.rationale}>
                                  Parar na L{stint.recommendation.target_lap} →{' '}
                                  {stint.recommendation.next_compound} (ganho{' '}
                                  {stint.recommendation.projected_gain_s.toFixed(1)} s)
                                </span>
                              ) : (
                                <span className="text-neutral-600">—</span>
                              )}
                            </td>
                          </tr>
                        );
                      })
                    )}
                  </tbody>
                </table>
              </div>
            )}
          </>
        )}

        {bottomTab === 'laps' && (
          <>
            {laps.error && <ErrorRow error={laps.error} />}
            {laps.loading && (
              <div className="flex justify-center py-8">
                <Loader2 className="h-4 w-4 animate-spin text-sky-500" />
              </div>
            )}
            {laps.data && (
              <div className="grid grid-cols-1 gap-4 md:grid-cols-2">
                {renderLapColumn(
                  `${refDriverCode || 'REF'} (#${refDriverNumber}) — voltas`,
                  'text-sky-400',
                  laps.data.ref
                )}
                {renderLapColumn(
                  `${compDriverCode || 'COMP'} (#${compDriverNumber}) — voltas`,
                  'text-rose-400',
                  laps.data.comp
                )}
              </div>
            )}
          </>
        )}
      </div>
    </div>
  );
};
