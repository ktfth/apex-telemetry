'use client';

import React from 'react';

/**
 * Animated skeleton loader matching ApexTelemetry's dark instrumentation theme.
 * Renders pulsing rectangles that mimic the shape of telemetry plots.
 */
export const TelemetrySkeleton: React.FC = () => {
  return (
    <div className="flex-1 flex flex-col gap-2 p-4 animate-pulse">
      {/* Speed plot skeleton */}
      <div className="flex-1 bg-[#161b24] rounded border border-[#232936]">
        <div className="h-3 w-20 bg-[#232936] rounded m-3" />
        <div className="mx-3 h-[60%] bg-[#1a2030] rounded" />
      </div>
      {/* Throttle/Brake plot skeleton */}
      <div className="flex-1 bg-[#161b24] rounded border border-[#232936]">
        <div className="h-3 w-28 bg-[#232936] rounded m-3" />
        <div className="mx-3 h-[60%] bg-[#1a2030] rounded" />
      </div>
      {/* Delta plot skeleton */}
      <div className="flex-1 bg-[#161b24] rounded border border-[#232936]">
        <div className="h-3 w-16 bg-[#232936] rounded m-3" />
        <div className="mx-3 h-[60%] bg-[#1a2030] rounded" />
      </div>
    </div>
  );
};

/**
 * Compact skeleton for the sidebar panels (LapSelector, InsightsPanel).
 */
export const PanelSkeleton: React.FC<{ lines?: number }> = ({ lines = 5 }) => {
  return (
    <div className="space-y-3 p-3 animate-pulse">
      {Array.from({ length: lines }).map((_, i) => (
        <div key={i} className="flex gap-2 items-center">
          <div className="h-3 bg-[#232936] rounded" style={{ width: `${60 + Math.random() * 30}%` }} />
        </div>
      ))}
    </div>
  );
};

/**
 * Inline loading indicator shown in the LapSelector delta section.
 */
export const DeltaLoadingIndicator: React.FC = () => {
  return (
    <span className="inline-flex items-center gap-1 text-sky-400/70">
      <span className="w-1.5 h-1.5 bg-sky-400 rounded-full animate-pulse" />
      <span className="text-[10px] font-mono uppercase tracking-wider">Computing…</span>
    </span>
  );
};
