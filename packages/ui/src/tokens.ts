/**
 * ApexTelemetry — Design Tokens
 * Engineering-grade telemetry visual language
 */

export const COLORS = {
  bg: {
    base: '#0c0e12',
    surface: '#12161c',
    surfaceHover: '#181d26',
    surfaceRaised: '#1d232e',
    subtleBorder: '#232936',
    activeBorder: '#3b4252'
  },
  text: {
    primary: '#f3f4f6',
    secondary: '#9ca3af',
    muted: '#6b7280',
    inverse: '#0f172a'
  },
  channel: {
    speed: '#38bdf8',      // Sky blue
    throttle: '#22c55e',   // Emerald green
    brake: '#ef4444',      // Red
    rpm: '#eab308',        // Amber
    gear: '#a855f7',       // Purple
    drs: '#06b6d4',        // Cyan
    deltaRef: '#38bdf8',   // Reference driver (P1)
    deltaComp: '#f97316'   // Comparison driver (P2)
  },
  tyres: {
    SOFT: '#ef4444',
    MEDIUM: '#eab308',
    HARD: '#f3f4f6',
    INTERMEDIATE: '#10b981',
    WET: '#3b82f6',
    UNKNOWN: '#6b7280'
  },
  status: {
    gain: '#22c55e',
    loss: '#ef4444',
    neutral: '#9ca3af',
    warning: '#f59e0b',
    flagYellow: '#eab308',
    flagRed: '#ef4444',
    flagGreen: '#22c55e'
  }
} as const;
