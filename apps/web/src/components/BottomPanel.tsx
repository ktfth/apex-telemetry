'use client';

import React from 'react';
import { useTelemetryStore } from '../store/telemetryStore';
import {
  DEMO_RACE_CONTROL,
  DEMO_STINTS,
  DEMO_LAPS_VER,
  DEMO_LAPS_LEC
} from '../fixtures/demoBahrain2024';
import { TyreBadge } from '@apex-telemetry/ui';
import { Radio, Disc, Timer, Flag } from 'lucide-react';

export const BottomPanel: React.FC = () => {
  const { bottomTab, setBottomTab } = useTelemetryStore();

  return (
    <div className="h-52 bg-[#0c0e12] border-t border-[#212836] flex flex-col select-none shrink-0 overflow-hidden">
      {/* Tab Navigation */}
      <div className="flex items-center space-x-1 border-b border-[#212836] bg-[#090b0e] px-3 pt-1">
        <button
          onClick={() => setBottomTab('race_control')}
          className={`flex items-center space-x-1.5 px-3 py-1.5 text-xs font-mono font-medium border-t-2 transition-colors ${
            bottomTab === 'race_control'
              ? 'border-sky-500 bg-[#12161c] text-neutral-100'
              : 'border-transparent text-neutral-400 hover:text-neutral-200'
          }`}
        >
          <Radio className="w-3 h-3 text-amber-400" />
          <span>DIREÇÃO DE PROVA & MENSAGENS</span>
        </button>

        <button
          onClick={() => setBottomTab('stints')}
          className={`flex items-center space-x-1.5 px-3 py-1.5 text-xs font-mono font-medium border-t-2 transition-colors ${
            bottomTab === 'stints'
              ? 'border-sky-500 bg-[#12161c] text-neutral-100'
              : 'border-transparent text-neutral-400 hover:text-neutral-200'
          }`}
        >
          <Disc className="w-3 h-3 text-rose-400" />
          <span>STINTS, PNEUS & DEGRADAÇÃO</span>
        </button>

        <button
          onClick={() => setBottomTab('laps')}
          className={`flex items-center space-x-1.5 px-3 py-1.5 text-xs font-mono font-medium border-t-2 transition-colors ${
            bottomTab === 'laps'
              ? 'border-sky-500 bg-[#12161c] text-neutral-100'
              : 'border-transparent text-neutral-400 hover:text-neutral-200'
          }`}
        >
          <Timer className="w-3 h-3 text-emerald-400" />
          <span>MATRIZ COMPLETA DE VOLTAS</span>
        </button>
      </div>

      {/* Tab Content Container */}
      <div className="flex-1 p-3 overflow-y-auto font-mono text-xs">
        {bottomTab === 'race_control' && (
          <div className="space-y-1.5">
            {DEMO_RACE_CONTROL.map((rc, idx) => (
              <div
                key={`rc-${idx}`}
                className="flex items-center justify-between p-2 bg-[#12161c] border border-[#1d232e]"
              >
                <div className="flex items-center space-x-3">
                  <span className="text-[10px] text-neutral-500 tabular-nums">
                    {rc.occurred_at.slice(11, 19)} UTC
                  </span>
                  <span
                    className={`px-1.5 py-0.5 text-[9px] font-bold border ${
                      rc.flag === 'YELLOW'
                        ? 'bg-amber-950/40 border-amber-600 text-amber-300'
                        : rc.flag === 'GREEN'
                        ? 'bg-emerald-950/40 border-emerald-600 text-emerald-300'
                        : 'bg-neutral-800 border-neutral-700 text-neutral-300'
                    }`}
                  >
                    {rc.flag}
                  </span>
                  <span className="text-neutral-200 text-xs font-sans font-medium">
                    {rc.message}
                  </span>
                </div>

                {rc.sector && (
                  <span className="text-[10px] text-neutral-400 px-1.5 py-0.5 bg-[#1b212c]">
                    SETOR {rc.sector}
                  </span>
                )}
              </div>
            ))}
          </div>
        )}

        {bottomTab === 'stints' && (
          <div className="overflow-x-auto">
            <table className="w-full text-left border-collapse">
              <thead>
                <tr className="border-b border-[#212836] text-[10px] text-neutral-400 uppercase tracking-wider">
                  <th className="py-1 px-2">Piloto</th>
                  <th className="py-1 px-2">Stint</th>
                  <th className="py-1 px-2">Composto</th>
                  <th className="py-1 px-2">Voltas</th>
                  <th className="py-1 px-2">Média (s)</th>
                  <th className="py-1 px-2">Degradação Estimada</th>
                  <th className="py-1 px-2">Pit Stop</th>
                </tr>
              </thead>
              <tbody className="divide-y divide-[#1b212c]">
                {DEMO_STINTS.map((st, idx) => (
                  <tr key={`stint-${idx}`} className="hover:bg-[#151a22]">
                    <td className="py-1 px-2 font-bold text-neutral-200">
                      {st.driver_number === 1 ? 'VER (#1)' : 'LEC (#16)'}
                    </td>
                    <td className="py-1 px-2">Stint {st.stint_number}</td>
                    <td className="py-1 px-2">
                      <TyreBadge compound={st.compound} laps={st.lap_count} />
                    </td>
                    <td className="py-1 px-2 tabular-nums">
                      L{st.lap_start} → L{st.lap_end} ({st.lap_count}v)
                    </td>
                    <td className="py-1 px-2 tabular-nums">{st.avg_lap_time_s.toFixed(2)}s</td>
                    <td className="py-1 px-2 tabular-nums text-amber-400">
                      +{st.estimated_degradation_s_per_lap.toFixed(3)} s/volta
                    </td>
                    <td className="py-1 px-2 tabular-nums text-neutral-400">
                      {st.pit_stop_duration_s ? `${st.pit_stop_duration_s.toFixed(1)}s` : '—'}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}

        {bottomTab === 'laps' && (
          <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
            <div>
              <div className="text-[10px] text-sky-400 font-bold uppercase mb-1">
                Max Verstappen (#1) — Histórico Q3
              </div>
              <div className="space-y-1">
                {DEMO_LAPS_VER.map((l) => (
                  <div key={`v-${l.lap_number}`} className="flex items-center justify-between p-1.5 bg-[#12161c] border border-[#1f2532]">
                    <span>Volta {l.lap_number}</span>
                    <span className="tabular-nums font-bold text-neutral-200">{l.lap_time_s.toFixed(3)}s</span>
                    <span className="text-[10px] text-neutral-500">Cob: {l.coverage_pct}%</span>
                  </div>
                ))}
              </div>
            </div>

            <div>
              <div className="text-[10px] text-rose-400 font-bold uppercase mb-1">
                Charles Leclerc (#16) — Histórico Q3
              </div>
              <div className="space-y-1">
                {DEMO_LAPS_LEC.map((l) => (
                  <div key={`l-${l.lap_number}`} className="flex items-center justify-between p-1.5 bg-[#12161c] border border-[#1f2532]">
                    <span>Volta {l.lap_number}</span>
                    <span className="tabular-nums font-bold text-neutral-200">{l.lap_time_s.toFixed(3)}s</span>
                    <span className="text-[10px] text-neutral-500">Cob: {l.coverage_pct}%</span>
                  </div>
                ))}
              </div>
            </div>
          </div>
        )}
      </div>
    </div>
  );
};
