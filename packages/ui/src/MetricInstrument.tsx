import React from 'react';

export interface MetricInstrumentProps {
  label: string;
  value: string | number;
  unit?: string;
  secondaryValue?: string | number;
  secondaryLabel?: string;
  variant?: 'default' | 'gain' | 'loss' | 'warning';
  annotation?: string;
  className?: string;
}

export const MetricInstrument: React.FC<MetricInstrumentProps> = ({
  label,
  value,
  unit,
  secondaryValue,
  secondaryLabel,
  variant = 'default',
  annotation,
  className = ''
}) => {
  const variantColor = {
    default: 'text-neutral-100',
    gain: 'text-emerald-400',
    loss: 'text-rose-400',
    warning: 'text-amber-400'
  }[variant];

  return (
    <div
      className={`border border-[#232936] bg-[#12161c] px-3 py-2 rounded-none flex flex-col justify-between select-none ${className}`}
    >
      <div className="flex items-center justify-between text-[10px] font-mono tracking-wider uppercase text-neutral-400 mb-1">
        <span>{label}</span>
        {annotation && (
          <span className="text-[9px] px-1 py-0.5 bg-[#1a202c] text-neutral-400 border border-[#2d3748]">
            {annotation}
          </span>
        )}
      </div>

      <div className="flex items-baseline space-x-1.5">
        <span className={`font-mono text-lg font-medium tabular-nums tracking-tight ${variantColor}`}>
          {value}
        </span>
        {unit && (
          <span className="font-mono text-xs text-neutral-400 font-normal">
            {unit}
          </span>
        )}
      </div>

      {secondaryValue !== undefined && (
        <div className="mt-1 flex items-center justify-between text-[11px] font-mono text-neutral-400 border-t border-[#1d232e] pt-1">
          <span>{secondaryLabel || 'COMP'}</span>
          <span className="tabular-nums text-neutral-300 font-medium">
            {secondaryValue}
          </span>
        </div>
      )}
    </div>
  );
};
