/**
 * ApexTelemetry — API Client for C++23 Backend Services
 * Connects to api-gateway-cpp with graceful fallback to offline/demo fixtures.
 */

import { Session, Driver, RaceControlEvent, LapComparison } from '@apex-telemetry/contracts';
import { DEMO_SESSIONS, DEMO_DRIVERS, DEMO_RACE_CONTROL, DEMO_LAP_COMPARISON } from '../fixtures/demoBahrain2024';

const API_BASE_URL = process.env.NEXT_PUBLIC_API_URL || 'http://localhost:8080/api/v1';

export interface ApiStatus {
  online: boolean;
  service?: string;
  version?: string;
  analyticsEngine?: string;
  latencyMs?: number;
}

export async function checkApiHealth(): Promise<ApiStatus> {
  const start = performance.now();
  try {
    const res = await fetch(`${API_BASE_URL}/health`, {
      method: 'GET',
      headers: { 'Accept': 'application/json' },
      signal: AbortSignal.timeout(1500)
    });
    if (!res.ok) return { online: false };
    const data = await res.json();
    const latencyMs = Math.round(performance.now() - start);
    return {
      online: true,
      service: data.service || 'apex-api-gateway',
      version: data.version || '1.5.0-fase5',
      analyticsEngine: data.analytics_engine || 'spatial_alignment_cpp23',
      latencyMs
    };
  } catch {
    return { online: false };
  }
}

export async function fetchSessions(): Promise<{ data: Session[]; isFromApi: boolean }> {
  try {
    const res = await fetch(`${API_BASE_URL}/sessions`, {
      method: 'GET',
      headers: { 'Accept': 'application/json' },
      signal: AbortSignal.timeout(2000)
    });
    if (res.ok) {
      const data = await res.json();
      if (Array.isArray(data) && data.length > 0) {
        return { data, isFromApi: true };
      }
    }
  } catch {
    // Falha de rede: fallback gracioso sem quebra de UI
  }
  return { data: DEMO_SESSIONS, isFromApi: false };
}

export async function fetchDrivers(sessionKey: number): Promise<{ data: Driver[]; isFromApi: boolean }> {
  try {
    const res = await fetch(`${API_BASE_URL}/sessions/${sessionKey}/drivers`, {
      method: 'GET',
      headers: { 'Accept': 'application/json' },
      signal: AbortSignal.timeout(2000)
    });
    if (res.ok) {
      const data = await res.json();
      if (Array.isArray(data) && data.length > 0) {
        return { data, isFromApi: true };
      }
    }
  } catch {
    // Fallback gracioso
  }
  return { data: DEMO_DRIVERS, isFromApi: false };
}

export async function fetchRaceControl(sessionKey: number): Promise<{ data: RaceControlEvent[]; isFromApi: boolean }> {
  try {
    const res = await fetch(`${API_BASE_URL}/sessions/${sessionKey}/race-control`, {
      method: 'GET',
      headers: { 'Accept': 'application/json' },
      signal: AbortSignal.timeout(2000)
    });
    if (res.ok) {
      const data = await res.json();
      if (Array.isArray(data) && data.length > 0) {
        return { data, isFromApi: true };
      }
    }
  } catch {
    // Fallback gracioso
  }
  return { data: DEMO_RACE_CONTROL, isFromApi: false };
}

export async function fetchLapComparison(
  sessionKey: number,
  refDriver: number,
  refLap: number,
  compDriver: number,
  compLap: number,
  stepM: number = 5.0
): Promise<{ data: LapComparison; isFromApi: boolean }> {
  try {
    const query = `session_key=${sessionKey}&ref_driver=${refDriver}&ref_lap=${refLap}&comp_driver=${compDriver}&comp_lap=${compLap}&step_m=${stepM}`;
    const res = await fetch(`${API_BASE_URL}/analysis/compare?${query}`, {
      method: 'GET',
      headers: { 'Accept': 'application/json' },
      signal: AbortSignal.timeout(3000)
    });
    if (res.ok) {
      const data = await res.json();
      if (data && data.channels && data.channels.length > 0) {
        return { data, isFromApi: true };
      }
    }
  } catch {
    // Fallback gracioso para fixture auditada
  }
  return { data: DEMO_LAP_COMPARISON, isFromApi: false };
}
