'use client';

import React, { useMemo } from 'react';
import { MapPin, Loader2, AlertTriangle } from 'lucide-react';
import { useTelemetryStore } from '../store/telemetryStore';

const VIEWPORT = 300;
const PADDING = 22;

/**
 * Mapa do circuito.
 *
 * O traçado é reconstruído pelo gateway a partir das amostras do transponder de
 * posição da volta mais rápida da sessão — não existe nenhuma coordenada de
 * circuito cadastrada no frontend. Qualquer circuito que a OpenF1 cubra aparece
 * aqui com a geometria que os carros realmente descreveram.
 */
export const CircuitMap: React.FC = () => {
  const { circuit, comparison, hoveredDistanceM, setHoveredDistanceM, activeInsightId } =
    useTelemetryStore();

  const geometry = circuit.data;

  const projection = useMemo(() => {
    if (!geometry || geometry.path.length < 2) return null;

    let minX = Infinity;
    let maxX = -Infinity;
    let minY = Infinity;
    let maxY = -Infinity;
    for (const [, x, y] of geometry.path) {
      if (x < minX) minX = x;
      if (x > maxX) maxX = x;
      if (y < minY) minY = y;
      if (y > maxY) maxY = y;
    }

    const width = Math.max(1, maxX - minX);
    const height = Math.max(1, maxY - minY);
    const scale = Math.min((VIEWPORT - 2 * PADDING) / width, (VIEWPORT - 2 * PADDING) / height);
    const offsetX = (VIEWPORT - width * scale) / 2;
    const offsetY = (VIEWPORT - height * scale) / 2;

    // O eixo Y do transponder aponta para cima; o do SVG aponta para baixo.
    const project = (x: number, y: number): [number, number] => [
      offsetX + (x - minX) * scale,
      VIEWPORT - offsetY - (y - minY) * scale
    ];

    const path = geometry.path
      .map(([, x, y], index) => {
        const [px, py] = project(x, y);
        return `${index === 0 ? 'M' : 'L'} ${px.toFixed(1)},${py.toFixed(1)}`;
      })
      .join(' ');

    return { project, path };
  }, [geometry]);

  /** Posição no traçado para uma distância arbitrária, por interpolação linear. */
  const positionAt = useMemo(() => {
    if (!geometry || !projection) return null;
    return (distanceM: number): [number, number] | null => {
      const points = geometry.path;
      if (points.length < 2) return null;
      const clamped = Math.max(points[0][0], Math.min(points[points.length - 1][0], distanceM));
      for (let i = 1; i < points.length; i += 1) {
        if (points[i][0] >= clamped) {
          const [d0, x0, y0] = points[i - 1];
          const [d1, x1, y1] = points[i];
          const span = d1 - d0;
          const alpha = span > 1e-6 ? (clamped - d0) / span : 0;
          return projection.project(x0 + alpha * (x1 - x0), y0 + alpha * (y1 - y0));
        }
      }
      const [, lastX, lastY] = points[points.length - 1];
      return projection.project(lastX, lastY);
    };
  }, [geometry, projection]);

  /** Trecho do insight ativo, destacado sobre o traçado real. */
  const insightPath = useMemo(() => {
    if (!geometry || !projection || !activeInsightId) return null;
    const insight = comparison.data?.insights.find((item) => item.id === activeInsightId);
    if (!insight) return null;

    const segment = geometry.path.filter(
      ([distance]) => distance >= insight.distance_start_m && distance <= insight.distance_end_m
    );
    if (segment.length < 2) return null;

    return segment
      .map(([, x, y], index) => {
        const [px, py] = projection.project(x, y);
        return `${index === 0 ? 'M' : 'L'} ${px.toFixed(1)},${py.toFixed(1)}`;
      })
      .join(' ');
  }, [geometry, projection, activeInsightId, comparison.data]);

  const cursor = hoveredDistanceM !== null && positionAt ? positionAt(hoveredDistanceM) : null;

  return (
    <div className="flex flex-col border border-[#212836] bg-[#0c0e12] p-3">
      <div className="mb-2 flex items-center justify-between font-mono text-[11px] text-neutral-400">
        <div className="flex items-center space-x-1.5 font-semibold text-neutral-200">
          <MapPin className="h-3.5 w-3.5 text-sky-400" />
          <span className="max-w-[210px] truncate uppercase" title={geometry?.circuit_name}>
            {geometry ? geometry.circuit_name : 'Traçado'}
          </span>
        </div>
        {geometry && (
          <span className="tabular-nums text-[10px] text-neutral-500">
            {Math.round(geometry.total_distance_m).toLocaleString('pt-BR')} m
          </span>
        )}
      </div>

      <div className="relative flex h-56 w-full items-center justify-center border border-[#1a202c] bg-[#090b0e]">
        {circuit.loading && (
          <div className="flex flex-col items-center gap-2 font-mono text-[11px] text-neutral-500">
            <Loader2 className="h-4 w-4 animate-spin text-sky-500" />
            <span>Reconstruindo o traçado a partir do transponder…</span>
          </div>
        )}

        {!circuit.loading && circuit.error && (
          <div className="flex max-w-[260px] flex-col items-center gap-1.5 px-4 text-center font-mono text-[10px] text-neutral-500">
            <AlertTriangle className="h-4 w-4 text-amber-500" />
            <span className="font-bold text-amber-400">{circuit.error.code}</span>
            <span className="leading-relaxed">{circuit.error.message}</span>
          </div>
        )}

        {!circuit.loading && !circuit.error && !geometry && (
          <span className="px-4 text-center font-mono text-[11px] text-neutral-600">
            Selecione uma sessão para reconstruir o traçado.
          </span>
        )}

        {geometry && projection && (
          <svg viewBox={`0 0 ${VIEWPORT} ${VIEWPORT}`} className="h-full w-full">
            <path
              d={projection.path}
              fill="none"
              stroke="#243044"
              strokeWidth="9"
              strokeLinecap="round"
              strokeLinejoin="round"
            />
            <path
              d={projection.path}
              fill="none"
              stroke="#0d1420"
              strokeWidth="5"
              strokeLinecap="round"
              strokeLinejoin="round"
            />

            {insightPath && (
              <path
                d={insightPath}
                fill="none"
                stroke="#f59e0b"
                strokeWidth="7"
                strokeOpacity="0.85"
                strokeLinecap="round"
                strokeLinejoin="round"
              />
            )}

            {geometry.corners.map((corner) => {
              const [cx, cy] = projection.project(corner.x, corner.y);
              return (
                <g
                  key={corner.label}
                  onMouseEnter={() => setHoveredDistanceM(corner.apex_distance_m)}
                  className="cursor-pointer"
                >
                  <circle cx={cx} cy={cy} r="3" fill="#38bdf8" />
                  <text
                    x={cx + 6}
                    y={cy + 3}
                    fill="#94a3b8"
                    fontSize="7.5"
                    fontFamily="JetBrains Mono, monospace"
                    fontWeight="bold"
                  >
                    {corner.label}
                  </text>
                  <title>
                    {`${corner.label} — ápice a ${Math.round(corner.apex_distance_m)} m, ${corner.apex_speed_kmh.toFixed(0)} km/h`}
                  </title>
                </g>
              );
            })}

            {cursor && (
              <circle cx={cursor[0]} cy={cursor[1]} r="4.5" fill="#38bdf8" stroke="#e2e8f0" strokeWidth="1.5" />
            )}
          </svg>
        )}

        {geometry && (
          <div className="absolute bottom-2 left-2 border border-[#212836] bg-[#0f141c]/90 px-2 py-1 font-mono text-[10px] text-neutral-400">
            {hoveredDistanceM !== null ? (
              <>
                POS: <span className="font-bold tabular-nums text-sky-300">{Math.round(hoveredDistanceM)} m</span>
              </>
            ) : (
              <span className="text-neutral-600">passe o cursor sobre os gráficos</span>
            )}
          </div>
        )}
      </div>

      {geometry && (
        <div className="mt-2 flex items-center justify-between font-mono text-[9px] text-neutral-600">
          <span>
            volta de referência: #{geometry.reference_driver_number} L{geometry.reference_lap_number} ·{' '}
            {geometry.reference_lap_time_s.toFixed(3)} s
          </span>
          <span className="uppercase">{geometry.data_source}</span>
        </div>
      )}
    </div>
  );
};
