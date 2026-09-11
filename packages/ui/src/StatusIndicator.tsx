import React from 'react';

export interface StatusIndicatorProps {
  status: 'live' | 'idle' | 'warning' | 'error' | 'synthetic';
  label?: string;
  className?: string;
}

export const StatusIndicator: React.FC<StatusIndicatorProps> = ({
  status,
  label,
  className = ''
}) => {
  const dotColor = {
    live: 'bg-emerald-500 animate-pulse',
    idle: 'bg-neutral-500',
    warning: 'bg-amber-500',
    error: 'bg-rose-500',
    synthetic: 'bg-indigo-400'
  }[status];

  return (
    <div className={`inline-flex items-center gap-1.5 font-mono text-[11px] text-neutral-300 ${className}`}>
      <span className={`w-1.5 h-1.5 rounded-full ${dotColor}`} />
      {label && <span className="uppercase tracking-wider">{label}</span>}
    </div>
  );
};
