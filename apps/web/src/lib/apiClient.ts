/**
 * ApexTelemetry — cliente do api-gateway-cpp.
 *
 * Há exatamente uma origem de dados: o gateway em C++23, que por sua vez resolve
 * PostgreSQL → OpenF1 ao vivo. Quando não há dado, este módulo propaga o erro
 * estruturado do gateway. Ele nunca sintetiza telemetria, nunca completa um campo
 * ausente com um valor plausível e nunca cai para um conjunto de demonstração: um
 * gráfico vazio é informação, um gráfico inventado é desinformação.
 */

import type {
  CircuitGeometry,
  DegradationReport,
  Driver,
  Lap,
  LapComparison,
  RaceControlEvent,
  Session,
  Stint,
  WeatherSample
} from '@apex-telemetry/contracts';

export const API_BASE_URL =
  process.env.NEXT_PUBLIC_API_URL?.replace(/\/+$/, '') || 'http://localhost:8080/api/v1';

/** Erro do gateway com o código legível por máquina preservado. */
export class ApiClientError extends Error {
  readonly code: string;
  readonly status: number;

  constructor(message: string, code: string, status: number) {
    super(message);
    this.name = 'ApiClientError';
    this.code = code;
    this.status = status;
  }

  /** Verdadeiro quando o gateway não está acessível (e não quando ele recusou o pedido). */
  get isOffline(): boolean {
    return this.status === 0;
  }
}

export interface ApiResult<T> {
  data: T;
  /** `postgresql`, `openf1-upstream`, ou o que o gateway declarar. */
  source: string;
  latencyMs: number;
}

const DEFAULT_TIMEOUT_MS = 15_000;
/** A comparação atravessa o pipeline completo de alinhamento espacial; leva mais tempo. */
const ANALYSIS_TIMEOUT_MS = 60_000;

function mergeSignals(external: AbortSignal | undefined, timeoutMs: number): AbortSignal {
  const timeout = AbortSignal.timeout(timeoutMs);
  if (!external) return timeout;
  // AbortSignal.any está disponível em todos os navegadores-alvo e no Node 20+.
  if (typeof AbortSignal.any === 'function') return AbortSignal.any([external, timeout]);
  return external;
}

async function request<T>(
  path: string,
  options: { signal?: AbortSignal; timeoutMs?: number } = {}
): Promise<ApiResult<T>> {
  const started = performance.now();
  let response: Response;

  try {
    response = await fetch(`${API_BASE_URL}${path}`, {
      method: 'GET',
      headers: { Accept: 'application/json' },
      signal: mergeSignals(options.signal, options.timeoutMs ?? DEFAULT_TIMEOUT_MS)
    });
  } catch (error) {
    // Um abort pedido pelo chamador precisa subir intacto para não virar erro de UI.
    if (options.signal?.aborted) throw error;
    const detail = error instanceof Error ? error.message : String(error);
    throw new ApiClientError(
      `Gateway inacessível em ${API_BASE_URL} (${detail}). Suba o api-gateway-cpp ou ajuste NEXT_PUBLIC_API_URL.`,
      'GATEWAY_UNREACHABLE',
      0
    );
  }

  if (!response.ok) {
    let code = `HTTP_${response.status}`;
    let message = `O gateway respondeu ${response.status} para ${path}`;
    try {
      const body = await response.json();
      if (body && typeof body === 'object') {
        if (typeof body.code === 'string') code = body.code;
        if (typeof body.error === 'string') message = body.error;
      }
    } catch {
      // Corpo não-JSON: mantemos a mensagem genérica com o status real.
    }
    throw new ApiClientError(message, code, response.status);
  }

  const data = (await response.json()) as T;
  return {
    data,
    source: response.headers.get('X-Apex-Data-Source') ?? 'unknown',
    latencyMs: Math.round(performance.now() - started)
  };
}

export interface GatewayHealth {
  online: boolean;
  version?: string;
  analyticsEngine?: string;
  database?: { configured: boolean; healthy: boolean };
  strategyEngine?: { configured: boolean; healthy: boolean };
  upstream?: { provider: string; base_url: string };
  cacheEntries?: number;
  uptimeSeconds?: number;
  latencyMs?: number;
  error?: string;
}

export async function checkGatewayHealth(signal?: AbortSignal): Promise<GatewayHealth> {
  try {
    const result = await request<{
      version: string;
      analytics_engine: string;
      database: { configured: boolean; healthy: boolean };
      strategy_engine: { configured: boolean; healthy: boolean };
      upstream: { provider: string; base_url: string };
      cache_entries: number;
      uptime_seconds: number;
    }>('/health', { signal, timeoutMs: 4_000 });

    return {
      online: true,
      version: result.data.version,
      analyticsEngine: result.data.analytics_engine,
      database: result.data.database,
      strategyEngine: result.data.strategy_engine,
      upstream: result.data.upstream,
      cacheEntries: result.data.cache_entries,
      uptimeSeconds: result.data.uptime_seconds,
      latencyMs: result.latencyMs
    };
  } catch (error) {
    return { online: false, error: error instanceof Error ? error.message : String(error) };
  }
}

export function fetchSessions(year: number, signal?: AbortSignal): Promise<ApiResult<Session[]>> {
  return request<Session[]>(`/sessions?year=${year}`, { signal, timeoutMs: 30_000 });
}

export function fetchDrivers(
  sessionKey: number,
  signal?: AbortSignal
): Promise<ApiResult<Driver[]>> {
  return request<Driver[]>(`/sessions/${sessionKey}/drivers`, { signal, timeoutMs: 30_000 });
}

export function fetchLaps(
  sessionKey: number,
  driverNumber: number,
  signal?: AbortSignal
): Promise<ApiResult<Lap[]>> {
  return request<Lap[]>(`/sessions/${sessionKey}/laps?driver_number=${driverNumber}`, {
    signal,
    timeoutMs: 30_000
  });
}

export function fetchStints(
  sessionKey: number,
  driverNumber: number | undefined,
  signal?: AbortSignal
): Promise<ApiResult<Stint[]>> {
  const query = driverNumber !== undefined ? `?driver_number=${driverNumber}` : '';
  return request<Stint[]>(`/sessions/${sessionKey}/stints${query}`, { signal, timeoutMs: 30_000 });
}

export function fetchRaceControl(
  sessionKey: number,
  signal?: AbortSignal
): Promise<ApiResult<RaceControlEvent[]>> {
  return request<RaceControlEvent[]>(`/sessions/${sessionKey}/race-control`, {
    signal,
    timeoutMs: 30_000
  });
}

export function fetchWeather(
  sessionKey: number,
  signal?: AbortSignal
): Promise<ApiResult<WeatherSample[]>> {
  return request<WeatherSample[]>(`/sessions/${sessionKey}/weather`, { signal, timeoutMs: 30_000 });
}

export function fetchCircuitGeometry(
  sessionKey: number,
  signal?: AbortSignal
): Promise<ApiResult<CircuitGeometry>> {
  return request<CircuitGeometry>(`/sessions/${sessionKey}/circuit`, {
    signal,
    timeoutMs: ANALYSIS_TIMEOUT_MS
  });
}

export interface ComparisonQuery {
  sessionKey: number;
  refDriver: number;
  refLap: number;
  compDriver: number;
  compLap: number;
  stepM?: number;
}

export function buildComparisonQuery(query: ComparisonQuery): string {
  const parameters = new URLSearchParams({
    session_key: String(query.sessionKey),
    ref_driver: String(query.refDriver),
    ref_lap: String(query.refLap),
    comp_driver: String(query.compDriver),
    comp_lap: String(query.compLap),
    step_m: String(query.stepM ?? 5.0)
  });
  return parameters.toString();
}

export function fetchLapComparison(
  query: ComparisonQuery,
  signal?: AbortSignal
): Promise<ApiResult<LapComparison>> {
  return request<LapComparison>(`/analysis/compare?${buildComparisonQuery(query)}`, {
    signal,
    timeoutMs: ANALYSIS_TIMEOUT_MS
  });
}

export function fetchDegradation(
  sessionKey: number,
  driverNumber: number,
  signal?: AbortSignal
): Promise<ApiResult<DegradationReport>> {
  return request<DegradationReport>(
    `/analysis/degradation?session_key=${sessionKey}&driver_number=${driverNumber}`,
    { signal, timeoutMs: ANALYSIS_TIMEOUT_MS }
  );
}

/** URL de download direto do gateway, para os botões de exportação. */
export function exportUrl(query: ComparisonQuery, format: 'motec_csv' | 'json'): string {
  return `${API_BASE_URL}/analysis/export?${buildComparisonQuery(query)}&format=${format}`;
}

/** URL do stream SSE de replay em tempo de pista. */
export function liveStreamUrl(query: ComparisonQuery, speed: number): string {
  return `${API_BASE_URL}/sessions/${query.sessionKey}/live?${buildComparisonQuery(query)}&speed=${speed}`;
}
