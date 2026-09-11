'use client';

import React from 'react';
import { useTelemetryStore } from '../store/telemetryStore';
import {
  BAHRAIN_CIRCUIT_COORDS,
  DEMO_LAP_COMPARISON
} from '../fixtures/demoBahrain2024';
import { MapPin } from 'lucide-react';

export const CircuitMap: React.FC = () => {
  const { hoveredDistanceM, setHoveredDistanceM, activeInsightId } = useTelemetryStore();
  const coords = BAHRAIN_CIRCUIT_COORDS;
  const totalDistance = DEMO_LAP_COMPARISON.total_distance_m;
  const activeDistance = hoveredDistanceM !== null ? hoveredDistanceM : 1550;

  // Interpola a posição do cursor (X, Y) na pista
  let currentCarPos = { x: coords[0].x, y: coords[0].y };

  for (let i = 0; i < coords.length - 1; i++) {
    const p1 = coords[i];
    const p2 = coords[i + 1];
    if (activeDistance >= p1.distanceM && activeDistance <= p2.distanceM) {
      const segDist = p2.distanceM - p1.distanceM;
      const t = segDist > 0 ? (activeDistance - p1.distanceM) / segDist : 0;
      currentCarPos = {
        x: p1.x + t * (p2.x - p1.x),
        y: p1.y + t * (p2.y - p1.y)
      };
      break;
    }
  }

  // Gera o path SVG do traçado
  const trackPath = coords
    .map((p, idx) => `${idx === 0 ? 'M' : 'L'} ${p.x},${p.y}`)
    .join(' ') + ' Z';

  // Trecho crítico do Insight (T4: 1420m - 2080m) destacado em vermelho/âmbar
  const insightCoords = coords.filter(
    (p) => p.distanceM >= 1400 && p.distanceM <= 2200
  );
  const insightPath = insightCoords.length > 1
    ? insightCoords.map((p, idx) => `${idx === 0 ? 'M' : 'L'} ${p.x},${p.y}`).join(' ')
    : '';

  return (
    <div className="flex flex-col border border-[#212836] bg-[#0c0e12] p-3">
      <div className="flex items-center justify-between text-[11px] font-mono text-neutral-400 mb-2">
        <div className="flex items-center space-x-1.5 font-semibold text-neutral-200">
          <MapPin className="w-3.5 h-3.5 text-sky-400" />
          <span>MAPA DO CIRCUITO — SAKHIR</span>
        </div>
        <span className="text-[10px] text-neutral-500 tabular-nums">5.412 m</span>
      </div>

      <div className="h-56 w-full relative flex items-center justify-center bg-[#090b0e] border border-[#1a202c]">
        <svg viewBox="150 120 420 460" className="w-full h-full p-2">
          {/* Fundo da pista (Linha base escura) */}
          <path
            d={trackPath}
            fill="none"
            stroke="#1f2937"
            strokeWidth="12"
            strokeLinecap="round"
            strokeLinejoin="round"
          />

          {/* Asfalto da pista */}
          <path
            d={trackPath}
            fill="none"
            stroke="#111827"
            strokeWidth="8"
            strokeLinecap="round"
            strokeLinejoin="round"
          />

          {/* Trecho destacado do Insight (Perda de tempo de Leclerc na T4) */}
          {activeInsightId === 'ins-t4-loss' && insightPath && (
            <path
              d={insightPath}
              fill="none"
              stroke="#ef4444"
              strokeWidth="10"
              strokeOpacity="0.7"
              strokeLinecap="round"
              strokeLinejoin="round"
            />
          )}

          {/* Marcadores de Curvas */}
          {coords
            .filter((c) => c.corner)
            .map((c) => (
              <g key={`corner-${c.corner}-${c.x}-${c.y}`}>
                <circle cx={c.x} cy={c.y} r="3" fill="#38bdf8" />
                <text
                  x={c.x + 8}
                  y={c.y + 3}
                  fill="#9ca3af"
                  fontSize="9"
                  fontFamily="JetBrains Mono, monospace"
                  fontWeight="bold"
                >
                  {c.corner}
                </text>
              </g>
            ))}

          {/* Posição do Carro no Traçado (Sincronizado ao Crosshair) */}
          <circle
            cx={currentCarPos.x}
            cy={currentCarPos.y}
            r="6"
            fill="#38bdf8"
            stroke="#ffffff"
            strokeWidth="2"
            className="animate-pulse"
          />
        </svg>

        {/* Coordenada e setor atual */}
        <div className="absolute bottom-2 left-2 px-2 py-1 bg-[#0f141c]/90 border border-[#212836] text-[10px] font-mono text-neutral-400">
          POS: <span className="text-sky-300 font-bold tabular-nums">{activeDistance} m</span>
        </div>
      </div>
    </div>
  );
};
