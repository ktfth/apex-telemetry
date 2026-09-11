'use client';

import React from 'react';
import { useTelemetryStore } from '../store/telemetryStore';
import {
  DEMO_DRIVERS,
  DEMO_LAPS_VER,
  DEMO_LAPS_LEC
} from '../fixtures/demoBahrain2024';
import { DriverBadge, TyreBadge, LapKindBadge } from '@apex-telemetry/ui';
import { SlidersHorizontal, Check } from 'lucide-react';

export const LapSelector: React.FC = () => {
  const {
    refDriverNumber,
    refLapNumber,
    compDriverNumber,
    compLapNumber,
    setRefSelection,
    setCompSelection,
    comparison,
    comparisonSource,
    comparisonLoading
  } = useTelemetryStore();

  const refDriver = DEMO_DRIVERS.find((d) => d.driver_number === refDriverNumber) || DEMO_DRIVERS[0];
  const compDriver = DEMO_DRIVERS.find((d) => d.driver_number === compDriverNumber) || DEMO_DRIVERS[1];

  const formatLapTime = (timeS: number) => {
    const mins = Math.floor(timeS / 60);
    const secs = (timeS % 60).toFixed(3);
    return `${mins}:${secs.padStart(6, '0')}`;
  };

  return (
    <aside className="w-72 bg-[#0d1015] border-r border-[#212836] flex flex-col h-full select-none shrink-0 overflow-y-auto">
      {/* Header da Coluna */}
      <div className="p-3 border-b border-[#212836] bg-[#11141a] flex items-center justify-between">
        <div className="flex items-center space-x-1.5 text-xs font-mono font-semibold text-neutral-300 uppercase tracking-wider">
          <SlidersHorizontal className="w-3.5 h-3.5 text-sky-400" />
          <span>Matriz de Voltas</span>
        </div>
        <span className="text-[10px] font-mono text-neutral-500">Q3 TIMING</span>
      </div>

      {/* Referência Primária (P1 / Linha de Base) */}
      <div className="p-3 border-b border-[#212836] bg-[#101319]">
        <div className="flex items-center justify-between mb-2">
          <span className="text-[10px] font-mono uppercase tracking-wider text-sky-400 font-bold">
            Referência [Base]
          </span>
          <DriverBadge
            driverCode={refDriver.name_acronym}
            driverNumber={refDriver.driver_number}
            teamColour={refDriver.team_colour}
          />
        </div>

        <div className="space-y-1.5">
          {DEMO_LAPS_VER.map((lap) => {
            const isSelected = lap.lap_number === refLapNumber;
            return (
              <button
                key={`ref-lap-${lap.lap_number}`}
                onClick={() => setRefSelection(refDriver.driver_number, lap.lap_number)}
                className={`w-full p-2 text-left font-mono text-xs border transition-colors flex items-center justify-between ${
                  isSelected
                    ? 'bg-[#182333] border-sky-600 text-sky-200'
                    : 'bg-[#12161c] border-[#212836] text-neutral-300 hover:bg-[#161b24]'
                }`}
              >
                <div>
                  <div className="flex items-center space-x-2">
                    <span className="font-bold">L{lap.lap_number}</span>
                    <span className="font-medium tabular-nums">{formatLapTime(lap.lap_time_s)}</span>
                  </div>
                  {lap.sector_1_s && (
                    <div className="text-[10px] text-neutral-500 mt-0.5 space-x-1.5">
                      <span>S1: {lap.sector_1_s.toFixed(3)}</span>
                      <span>S2: {lap.sector_2_s?.toFixed(3)}</span>
                      <span>S3: {lap.sector_3_s?.toFixed(3)}</span>
                    </div>
                  )}
                </div>

                <div className="flex items-center space-x-1.5">
                  <TyreBadge compound={lap.compound} laps={lap.stint_number} />
                  <LapKindBadge kind={lap.lap_kind} />
                </div>
              </button>
            );
          })}
        </div>
      </div>

      {/* Piloto de Comparação (P2) */}
      <div className="p-3 border-b border-[#212836] bg-[#101319] flex-1">
        <div className="flex items-center justify-between mb-2">
          <span className="text-[10px] font-mono uppercase tracking-wider text-rose-400 font-bold">
            Comparação [Delta]
          </span>
          <DriverBadge
            driverCode={compDriver.name_acronym}
            driverNumber={compDriver.driver_number}
            teamColour={compDriver.team_colour}
          />
        </div>

        <div className="space-y-1.5">
          {DEMO_LAPS_LEC.map((lap) => {
            const isSelected = lap.lap_number === compLapNumber;
            return (
              <button
                key={`comp-lap-${lap.lap_number}`}
                onClick={() => setCompSelection(compDriver.driver_number, lap.lap_number)}
                className={`w-full p-2 text-left font-mono text-xs border transition-colors flex items-center justify-between ${
                  isSelected
                    ? 'bg-[#2b181d] border-rose-600 text-rose-200'
                    : 'bg-[#12161c] border-[#212836] text-neutral-300 hover:bg-[#161b24]'
                }`}
              >
                <div>
                  <div className="flex items-center space-x-2">
                    <span className="font-bold">L{lap.lap_number}</span>
                    <span className="font-medium tabular-nums">{formatLapTime(lap.lap_time_s)}</span>
                  </div>
                  {lap.sector_1_s && (
                    <div className="text-[10px] text-neutral-500 mt-0.5 space-x-1.5">
                      <span>S1: {lap.sector_1_s.toFixed(3)}</span>
                      <span>S2: {lap.sector_2_s?.toFixed(3)}</span>
                      <span>S3: {lap.sector_3_s?.toFixed(3)}</span>
                    </div>
                  )}
                </div>

                <div className="flex items-center space-x-1.5">
                  <TyreBadge compound={lap.compound} laps={lap.stint_number} />
                  <LapKindBadge kind={lap.lap_kind} />
                </div>
              </button>
            );
          })}
        </div>
      </div>

      {/* Resumo Rápido da Comparação Ativa */}
      <div className="p-3 bg-[#0a0c10] border-t border-[#212836] text-xs font-mono">
        <div className="text-[10px] text-neutral-500 uppercase tracking-wider mb-1">
          Delta Oficial de Tempo
        </div>
        <div className="flex items-baseline justify-between">
          <span className="text-neutral-400">{comparison.reference_lap.driver_code} L{comparison.reference_lap.lap_number} vs {comparison.comparison_lap.driver_code} L{comparison.comparison_lap.lap_number}:</span>
          <span className="text-rose-400 font-bold tabular-nums">{comparisonLoading ? '…' : `${comparison.comparison_lap.lap_time_s - comparison.reference_lap.lap_time_s >= 0 ? '+' : ''}${(comparison.comparison_lap.lap_time_s - comparison.reference_lap.lap_time_s).toFixed(3)} s`}</span>
        </div>
        <div className="text-[10px] text-neutral-500 mt-1">
          Cobertura: {comparison.reference_lap.coverage_pct.toFixed(1)}% vs {comparison.comparison_lap.coverage_pct.toFixed(1)}% ({comparison.channels.length} nós) · {comparisonSource.toUpperCase()}
        </div>
      </div>
    </aside>
  );
};
