'use client';

import React, { useEffect, useMemo, useState } from 'react';
import { AlertTriangle, CalendarDays, Loader2, RefreshCcw, SlidersHorizontal } from 'lucide-react';
import { DriverBadge, TyreBadge, LapKindBadge } from '@apex-telemetry/ui';
import type { Driver, Lap, Session } from '@apex-telemetry/contracts';
import { ApiClientError, exportUrl, fetchDrivers, fetchLaps, fetchSessions } from '../lib/apiClient';
import { useTelemetryStore } from '../store/telemetryStore';

/** Anos com cobertura de telemetria de carro na OpenF1. */
const AVAILABLE_YEARS = [2025, 2024, 2023];

interface Remote<T> {
  data: T | null;
  loading: boolean;
  error: ApiClientError | null;
}

function useRemote<T>(
  loader: ((signal: AbortSignal) => Promise<{ data: T }>) | null,
  dependencies: React.DependencyList
): Remote<T> {
  const [state, setState] = useState<Remote<T>>({ data: null, loading: loader !== null, error: null });

  useEffect(() => {
    if (!loader) {
      setState({ data: null, loading: false, error: null });
      return;
    }
    const controller = new AbortController();
    setState({ data: null, loading: true, error: null });

    loader(controller.signal)
      .then((result) => {
        if (controller.signal.aborted) return;
        setState({ data: result.data, loading: false, error: null });
      })
      .catch((error: unknown) => {
        if (controller.signal.aborted) return;
        setState({
          data: null,
          loading: false,
          error:
            error instanceof ApiClientError
              ? error
              : new ApiClientError(String(error), 'UNEXPECTED_ERROR', 0)
        });
      });

    return () => controller.abort();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, dependencies);

  return state;
}

function formatLapTime(seconds: number | null): string {
  if (seconds === null || seconds <= 0) return '—';
  const minutes = Math.floor(seconds / 60);
  const remainder = (seconds % 60).toFixed(3);
  return `${minutes}:${remainder.padStart(6, '0')}`;
}

/** Voltas cronometradas: as únicas comparáveis espacialmente. */
function timedLaps(laps: Lap[] | null): Lap[] {
  if (!laps) return [];
  return laps.filter((lap) => lap.lap_time_s !== null && lap.lap_time_s > 0);
}

const ErrorNote: React.FC<{ error: ApiClientError }> = ({ error }) => (
  <div className="flex flex-col items-center gap-1 p-3 text-center font-mono text-[10px] text-neutral-500">
    <AlertTriangle className="h-4 w-4 text-amber-500" />
    <span className="font-bold text-amber-400">{error.code}</span>
    <span className="leading-relaxed">{error.message}</span>
  </div>
);

export const LapSelector: React.FC = () => {
  const {
    sessionYear,
    setSessionYear,
    sessionKey,
    setSession,
    refDriverNumber,
    refLapNumber,
    refDriverCode,
    compDriverNumber,
    compLapNumber,
    compDriverCode,
    setRefSelection,
    setCompSelection,
    comparison,
    comparisonQuery
  } = useTelemetryStore();

  const sessions = useRemote<Session[]>((signal) => fetchSessions(sessionYear, signal), [sessionYear]);

  // Seleciona a sessão mais recente do ano assim que o catálogo chega.
  useEffect(() => {
    const list = sessions.data;
    if (!list || list.length === 0) return;
    const active = list.find((item) => item.session_key === sessionKey) ?? list[0];
    setSession(active.session_key, `${active.country_name} · ${active.session_name}`);
  }, [sessions.data, sessionKey, setSession]);

  const drivers = useRemote<Driver[]>(
    sessionKey === null ? null : (signal) => fetchDrivers(sessionKey, signal),
    [sessionKey]
  );

  // Dois pilotos distintos por padrão; sem inventar números que não correram.
  useEffect(() => {
    const list = drivers.data;
    if (!list || list.length === 0) return;

    const refPresent = list.some((driver) => driver.driver_number === refDriverNumber);
    if (!refPresent) {
      setRefSelection(list[0].driver_number, null, list[0].name_acronym);
    }
    const compPresent = list.some((driver) => driver.driver_number === compDriverNumber);
    if (!compPresent) {
      const alternative = list.find((driver) => driver.driver_number !== list[0].driver_number) ?? list[0];
      setCompSelection(alternative.driver_number, null, alternative.name_acronym);
    }
  }, [drivers.data, refDriverNumber, compDriverNumber, setRefSelection, setCompSelection]);

  const refLaps = useRemote<Lap[]>(
    sessionKey === null || refDriverNumber === null
      ? null
      : (signal) => fetchLaps(sessionKey, refDriverNumber, signal),
    [sessionKey, refDriverNumber]
  );

  const compLaps = useRemote<Lap[]>(
    sessionKey === null || compDriverNumber === null
      ? null
      : (signal) => fetchLaps(sessionKey, compDriverNumber, signal),
    [sessionKey, compDriverNumber]
  );

  // A volta mais rápida de cada piloto é o ponto de partida natural da análise.
  useEffect(() => {
    const laps = timedLaps(refLaps.data);
    if (laps.length === 0 || refDriverNumber === null) return;
    if (laps.some((lap) => lap.lap_number === refLapNumber)) return;
    const fastest = laps.reduce((best, lap) => (lap.lap_time_s! < best.lap_time_s! ? lap : best));
    setRefSelection(refDriverNumber, fastest.lap_number);
  }, [refLaps.data, refDriverNumber, refLapNumber, setRefSelection]);

  useEffect(() => {
    const laps = timedLaps(compLaps.data);
    if (laps.length === 0 || compDriverNumber === null) return;
    if (laps.some((lap) => lap.lap_number === compLapNumber)) return;
    const fastest = laps.reduce((best, lap) => (lap.lap_time_s! < best.lap_time_s! ? lap : best));
    setCompSelection(compDriverNumber, fastest.lap_number);
  }, [compLaps.data, compDriverNumber, compLapNumber, setCompSelection]);

  const refDriver = drivers.data?.find((driver) => driver.driver_number === refDriverNumber);
  const compDriver = drivers.data?.find((driver) => driver.driver_number === compDriverNumber);
  const query = comparisonQuery();

  const officialDelta = useMemo(() => {
    const data = comparison.data;
    if (!data) return null;
    return data.comparison_lap.lap_time_s - data.reference_lap.lap_time_s;
  }, [comparison.data]);

  const renderLaps = (
    remote: Remote<Lap[]>,
    selectedLap: number | null,
    onSelect: (lapNumber: number) => void,
    accent: 'sky' | 'rose'
  ) => {
    if (remote.loading) {
      return (
        <div className="space-y-2 p-2">
          {[0, 1, 2].map((index) => (
            <div key={index} className="h-10 animate-pulse border border-[#212836] bg-[#161b24]" />
          ))}
        </div>
      );
    }
    if (remote.error) return <ErrorNote error={remote.error} />;

    const laps = timedLaps(remote.data);
    if (laps.length === 0) {
      return (
        <div className="flex flex-col items-center p-3 text-center text-[11px] text-neutral-500">
          <AlertTriangle className="mb-1 h-4 w-4 text-amber-500 opacity-60" />
          Nenhuma volta cronometrada para este piloto nesta sessão.
        </div>
      );
    }

    const fastest = laps.reduce((best, lap) => (lap.lap_time_s! < best.lap_time_s! ? lap : best));

    return (
      <div className="max-h-[220px] space-y-1.5 overflow-y-auto p-2">
        {laps.map((lap) => {
          const selected = lap.lap_number === selectedLap;
          const isReference = accent === 'sky';
          return (
            <button
              key={lap.lap_number}
              onClick={() => onSelect(lap.lap_number)}
              className={`flex w-full items-center justify-between border p-2 text-left font-mono text-xs transition-colors ${
                selected
                  ? isReference
                    ? 'border-sky-600 bg-[#182333] text-sky-200'
                    : 'border-rose-600 bg-[#2b181d] text-rose-200'
                  : 'border-[#212836] bg-[#12161c] text-neutral-300 hover:bg-[#161b24]'
              }`}
            >
              <div>
                <div className="flex items-center space-x-2">
                  <span className="font-bold">L{lap.lap_number}</span>
                  <span className="font-medium tabular-nums">{formatLapTime(lap.lap_time_s)}</span>
                  {lap.lap_number === fastest.lap_number && (
                    <span className="bg-violet-950/60 px-1 text-[9px] font-bold text-violet-300">
                      MELHOR
                    </span>
                  )}
                </div>
                {(lap.sector_1_s || lap.sector_2_s || lap.sector_3_s) && (
                  <div className="mt-0.5 space-x-1.5 text-[10px] text-neutral-500">
                    {lap.sector_1_s !== null && <span>S1 {lap.sector_1_s.toFixed(3)}</span>}
                    {lap.sector_2_s !== null && <span>S2 {lap.sector_2_s.toFixed(3)}</span>}
                    {lap.sector_3_s !== null && <span>S3 {lap.sector_3_s.toFixed(3)}</span>}
                  </div>
                )}
              </div>
              <div className="flex shrink-0 items-center space-x-1.5">
                <TyreBadge compound={lap.compound} ageLaps={lap.tyre_age_laps} />
                <LapKindBadge kind={lap.lap_kind} />
              </div>
            </button>
          );
        })}
      </div>
    );
  };

  return (
    <aside className="flex h-full w-80 shrink-0 select-none flex-col border-r border-[#212836] bg-[#0d1015]">
      <div className="flex flex-col space-y-3 border-b border-[#212836] bg-[#11141a] p-3">
        <div className="flex items-center justify-between">
          <div className="flex items-center space-x-1.5 font-mono text-xs font-semibold uppercase tracking-wider text-neutral-300">
            <SlidersHorizontal className="h-3.5 w-3.5 text-sky-400" />
            <span>Matriz de evento</span>
          </div>
          {comparison.loading && <RefreshCcw className="h-3.5 w-3.5 animate-spin text-neutral-500" />}
        </div>

        <div className="flex space-x-2">
          <div className="flex w-24 shrink-0 flex-col">
            <label className="mb-1 flex items-center gap-1 font-mono text-[9px] uppercase text-neutral-500">
              <CalendarDays className="h-2.5 w-2.5" /> Ano
            </label>
            <select
              value={sessionYear}
              onChange={(event) => setSessionYear(Number(event.target.value))}
              className="w-full rounded border border-[#212836] bg-[#161b24] p-1 font-mono text-xs text-neutral-300 outline-none focus:border-sky-500"
            >
              {AVAILABLE_YEARS.map((year) => (
                <option key={year} value={year}>
                  {year}
                </option>
              ))}
            </select>
          </div>

          <div className="flex min-w-0 flex-1 flex-col">
            <label className="mb-1 font-mono text-[9px] uppercase text-neutral-500">Sessão</label>
            <select
              value={sessionKey ?? ''}
              onChange={(event) => {
                const key = Number(event.target.value);
                const session = sessions.data?.find((item) => item.session_key === key);
                if (session) setSession(key, `${session.country_name} · ${session.session_name}`);
              }}
              disabled={sessions.loading || !!sessions.error}
              className="w-full truncate rounded border border-[#212836] bg-[#161b24] p-1 font-mono text-xs text-neutral-300 outline-none focus:border-sky-500"
            >
              {sessions.loading && <option>Carregando {sessionYear}…</option>}
              {sessions.error && <option>Indisponível</option>}
              {sessions.data?.map((session) => (
                <option key={session.session_key} value={session.session_key}>
                  {session.country_name} — {session.session_name}
                </option>
              ))}
            </select>
          </div>
        </div>

        {sessions.error && <ErrorNote error={sessions.error} />}
      </div>

      <div className="flex-1 overflow-y-auto">
        <div className="border-b border-[#212836] bg-[#101319]">
          <div className="flex items-center justify-between gap-2 p-3 pb-2">
            <span className="shrink-0 font-mono text-[10px] font-bold uppercase tracking-wider text-sky-400">
              Referência
            </span>
            <select
              value={refDriverNumber ?? ''}
              onChange={(event) => {
                const number = Number(event.target.value);
                const driver = drivers.data?.find((item) => item.driver_number === number);
                setRefSelection(number, null, driver?.name_acronym);
              }}
              disabled={drivers.loading || !!drivers.error}
              className="max-w-[190px] flex-1 truncate rounded border border-[#212836] bg-[#161b24] p-1 font-mono text-xs text-neutral-200 outline-none"
            >
              {drivers.loading && <option>Carregando pilotos…</option>}
              {drivers.data?.map((driver) => (
                <option key={driver.driver_number} value={driver.driver_number}>
                  {driver.name_acronym} · #{driver.driver_number}
                </option>
              ))}
            </select>
          </div>
          {refDriver && (
            <div className="flex items-center justify-between px-3 pb-1 font-mono text-[10px] text-neutral-500">
              <span className="truncate">{refDriver.team_name}</span>
              <DriverBadge
                driverCode={refDriver.name_acronym}
                driverNumber={refDriver.driver_number}
                teamColour={refDriver.team_colour}
              />
            </div>
          )}
          {renderLaps(
            refLaps,
            refLapNumber,
            (lap) => refDriverNumber !== null && setRefSelection(refDriverNumber, lap),
            'sky'
          )}
        </div>

        <div className="bg-[#101319]">
          <div className="flex items-center justify-between gap-2 p-3 pb-2">
            <span className="shrink-0 font-mono text-[10px] font-bold uppercase tracking-wider text-rose-400">
              Comparação
            </span>
            <select
              value={compDriverNumber ?? ''}
              onChange={(event) => {
                const number = Number(event.target.value);
                const driver = drivers.data?.find((item) => item.driver_number === number);
                setCompSelection(number, null, driver?.name_acronym);
              }}
              disabled={drivers.loading || !!drivers.error}
              className="max-w-[190px] flex-1 truncate rounded border border-[#212836] bg-[#161b24] p-1 font-mono text-xs text-neutral-200 outline-none"
            >
              {drivers.loading && <option>Carregando pilotos…</option>}
              {drivers.data?.map((driver) => (
                <option key={driver.driver_number} value={driver.driver_number}>
                  {driver.name_acronym} · #{driver.driver_number}
                </option>
              ))}
            </select>
          </div>
          {compDriver && (
            <div className="flex items-center justify-between px-3 pb-1 font-mono text-[10px] text-neutral-500">
              <span className="truncate">{compDriver.team_name}</span>
              <DriverBadge
                driverCode={compDriver.name_acronym}
                driverNumber={compDriver.driver_number}
                teamColour={compDriver.team_colour}
              />
            </div>
          )}
          {renderLaps(
            compLaps,
            compLapNumber,
            (lap) => compDriverNumber !== null && setCompSelection(compDriverNumber, lap),
            'rose'
          )}
        </div>
      </div>

      <div className="mt-auto shrink-0 border-t border-[#212836] bg-[#0a0c10] p-3 font-mono text-xs">
        <div className="mb-1 text-[10px] uppercase tracking-wider text-neutral-500">
          Delta cronometrado oficial
        </div>
        {comparison.loading ? (
          <div className="flex items-center gap-2 text-neutral-500">
            <Loader2 className="h-3 w-3 animate-spin" />
            <span className="text-[11px]">alinhando as duas voltas…</span>
          </div>
        ) : comparison.data && officialDelta !== null ? (
          <>
            <div className="flex items-baseline justify-between">
              <span className="truncate text-neutral-400">
                {comparison.data.reference_lap.driver_code} L{comparison.data.reference_lap.lap_number}{' '}
                vs {comparison.data.comparison_lap.driver_code} L
                {comparison.data.comparison_lap.lap_number}
              </span>
              <span
                className={`font-bold tabular-nums ${officialDelta >= 0 ? 'text-rose-400' : 'text-emerald-400'}`}
              >
                {officialDelta >= 0 ? '+' : ''}
                {officialDelta.toFixed(3)} s
              </span>
            </div>
            <div className="mt-1 text-[10px] text-neutral-500">
              Cobertura {comparison.data.reference_lap.coverage_pct.toFixed(1)}% ·{' '}
              {comparison.data.comparison_lap.coverage_pct.toFixed(1)}% ·{' '}
              {comparison.data.channels.length} nós · {comparison.data.data_source}
            </div>
          </>
        ) : comparison.error ? (
          <div className="text-[10px] leading-relaxed text-amber-400">
            <span className="font-bold">{comparison.error.code}</span> — {comparison.error.message}
          </div>
        ) : (
          <div className="text-[11px] text-neutral-600">
            Escolha dois pilotos e duas voltas cronometradas.
          </div>
        )}

        <div className="mt-2 flex items-center space-x-2 border-t border-[#1a202c] pt-2">
          <a
            href={query ? exportUrl(query, 'motec_csv') : undefined}
            aria-disabled={!query}
            className={`flex-1 border border-[#232936] py-1 text-center text-[10px] font-bold uppercase tracking-wider transition-colors ${
              query
                ? 'bg-[#161b24] text-sky-400 hover:bg-[#202634]'
                : 'pointer-events-none bg-[#101319] text-neutral-700'
            }`}
          >
            MoTeC CSV
          </a>
          <a
            href={query ? exportUrl(query, 'json') : undefined}
            aria-disabled={!query}
            className={`flex-1 border border-[#232936] py-1 text-center text-[10px] font-bold uppercase tracking-wider transition-colors ${
              query
                ? 'bg-[#161b24] text-neutral-300 hover:bg-[#202634]'
                : 'pointer-events-none bg-[#101319] text-neutral-700'
            }`}
          >
            JSON
          </a>
        </div>
      </div>
    </aside>
  );
};
