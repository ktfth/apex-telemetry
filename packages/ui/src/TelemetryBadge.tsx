import React from 'react';
import { TyreCompound, LapKind } from '@apex-telemetry/contracts';
import { COLORS } from './tokens';

export interface TyreBadgeProps {
  compound: TyreCompound;
  laps?: number;
  className?: string;
}

export const TyreBadge: React.FC<TyreBadgeProps> = ({ compound, laps, className = '' }) => {
  const color = COLORS.tyres[compound] || COLORS.tyres.UNKNOWN;
  const initial = compound.charAt(0).toUpperCase();

  return (
    <div
      className={`inline-flex items-center gap-1.5 px-2 py-0.5 border border-[#232936] bg-[#12161c] text-xs font-mono font-medium ${className}`}
      title={`Composto: ${compound}${laps !== undefined ? ` (${laps} voltas)` : ''}`}
    >
      <span
        className="w-2.5 h-2.5 rounded-full inline-block"
        style={{ backgroundColor: color }}
      />
      <span className="text-neutral-200">{initial}</span>
      {laps !== undefined && (
        <span className="text-neutral-500 text-[10px] tabular-nums">L{laps}</span>
      )}
    </div>
  );
};

export interface DriverBadgeProps {
  driverCode: string;
  driverNumber: number;
  teamColour: string;
  className?: string;
}

export const DriverBadge: React.FC<DriverBadgeProps> = ({
  driverCode,
  driverNumber,
  teamColour,
  className = ''
}) => {
  return (
    <div
      className={`inline-flex items-center gap-1.5 px-2 py-0.5 bg-[#12161c] border border-[#232936] font-mono text-xs ${className}`}
      style={{ borderLeftColor: teamColour, borderLeftWidth: 3 }}
    >
      <span className="text-neutral-400 text-[11px] tabular-nums font-semibold">
        #{driverNumber}
      </span>
      <span className="text-neutral-100 font-bold tracking-wide">
        {driverCode}
      </span>
    </div>
  );
};

export interface LapKindBadgeProps {
  kind: LapKind;
  className?: string;
}

export const LapKindBadge: React.FC<LapKindBadgeProps> = ({ kind, className = '' }) => {
  const labelMap: Record<LapKind, { text: string; color: string }> = {
    FLYING: { text: 'FLY', color: 'text-emerald-400 border-emerald-900/60 bg-emerald-950/20' },
    OUT_LAP: { text: 'OUT', color: 'text-neutral-400 border-neutral-800 bg-neutral-900/40' },
    IN_LAP: { text: 'IN', color: 'text-amber-400 border-amber-900/60 bg-amber-950/20' },
    INVALID: { text: 'INV', color: 'text-rose-400 border-rose-900/60 bg-rose-950/20' },
    SAFETY_CAR: { text: 'SC', color: 'text-yellow-400 border-yellow-900/60 bg-yellow-950/20' }
  };

  const item = labelMap[kind] || { text: kind, color: 'text-neutral-400 border-neutral-800' };

  return (
    <span
      className={`inline-block px-1.5 py-0.5 text-[10px] font-mono font-medium border uppercase tracking-wider ${item.color} ${className}`}
    >
      {item.text}
    </span>
  );
};
