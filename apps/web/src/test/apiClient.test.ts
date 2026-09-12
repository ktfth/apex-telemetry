import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import {
  ApiClientError,
  buildComparisonQuery,
  checkGatewayHealth,
  exportUrl,
  fetchLapComparison,
  fetchSessions,
  liveStreamUrl
} from '../lib/apiClient';

const QUERY = {
  sessionKey: 9468,
  refDriver: 1,
  refLap: 16,
  compDriver: 16,
  compLap: 15,
  stepM: 5.0
};

function jsonResponse(body: unknown, init: { status?: number; source?: string } = {}): Response {
  const headers = new Headers({ 'Content-Type': 'application/json' });
  if (init.source) headers.set('X-Apex-Data-Source', init.source);
  return new Response(JSON.stringify(body), { status: init.status ?? 200, headers });
}

describe('apiClient', () => {
  beforeEach(() => {
    vi.stubGlobal('fetch', vi.fn());
  });

  afterEach(() => {
    vi.unstubAllGlobals();
    vi.restoreAllMocks();
  });

  it('monta a query da comparação com todos os parâmetros que o gateway exige', () => {
    const query = new URLSearchParams(buildComparisonQuery(QUERY));
    expect(query.get('session_key')).toBe('9468');
    expect(query.get('ref_driver')).toBe('1');
    expect(query.get('ref_lap')).toBe('16');
    expect(query.get('comp_driver')).toBe('16');
    expect(query.get('comp_lap')).toBe('15');
    expect(query.get('step_m')).toBe('5');
  });

  it('exportação e stream apontam para a mesma seleção da comparação', () => {
    expect(exportUrl(QUERY, 'motec_csv')).toContain('format=motec_csv');
    expect(exportUrl(QUERY, 'json')).toContain('format=json');
    expect(exportUrl(QUERY, 'json')).toContain('ref_lap=16');
    expect(liveStreamUrl(QUERY, 10)).toContain('/sessions/9468/live?');
    expect(liveStreamUrl(QUERY, 10)).toContain('speed=10');
  });

  it('propaga o payload e a origem declarada pelo gateway', async () => {
    const sessions = [{ session_key: 9468, session_name: 'Qualifying' }];
    vi.mocked(fetch).mockResolvedValue(jsonResponse(sessions, { source: 'postgresql' }));

    const result = await fetchSessions(2024);
    expect(result.data).toEqual(sessions);
    expect(result.source).toBe('postgresql');
    expect(vi.mocked(fetch).mock.calls[0][0]).toContain('/sessions?year=2024');
  });

  it('preserva o código de erro estruturado do gateway em vez de mascará-lo', async () => {
    vi.mocked(fetch).mockResolvedValue(
      jsonResponse(
        { error: 'Lap 99 of driver 1 has no recorded lap time', code: 'LAP_NOT_FOUND' },
        { status: 404 }
      )
    );

    const failure = await fetchLapComparison(QUERY).catch((error: unknown) => error);
    expect(failure).toBeInstanceOf(ApiClientError);
    expect((failure as ApiClientError).code).toBe('LAP_NOT_FOUND');
    expect((failure as ApiClientError).status).toBe(404);
    expect((failure as ApiClientError).message).toContain('no recorded lap time');
    expect((failure as ApiClientError).isOffline).toBe(false);
  });

  it('nunca devolve dados de demonstração quando o gateway está inacessível', async () => {
    vi.mocked(fetch).mockRejectedValue(new TypeError('Failed to fetch'));

    const failure = await fetchLapComparison(QUERY).catch((error: unknown) => error);
    expect(failure).toBeInstanceOf(ApiClientError);
    expect((failure as ApiClientError).code).toBe('GATEWAY_UNREACHABLE');
    expect((failure as ApiClientError).isOffline).toBe(true);
  });

  it('lida com um corpo de erro que não é JSON sem estourar', async () => {
    vi.mocked(fetch).mockResolvedValue(
      new Response('<html>502 Bad Gateway</html>', { status: 502 })
    );

    const failure = await fetchSessions(2024).catch((error: unknown) => error);
    expect(failure).toBeInstanceOf(ApiClientError);
    expect((failure as ApiClientError).code).toBe('HTTP_502');
    expect((failure as ApiClientError).status).toBe(502);
  });

  it('reporta o gateway offline sem lançar, para que o cabeçalho possa exibir o estado', async () => {
    vi.mocked(fetch).mockRejectedValue(new TypeError('connection refused'));

    const health = await checkGatewayHealth();
    expect(health.online).toBe(false);
    expect(health.error).toBeTruthy();
  });

  it('repassa o estado real de banco e motor de estratégia reportado pelo gateway', async () => {
    vi.mocked(fetch).mockResolvedValue(
      jsonResponse({
        version: '2.0.0',
        analytics_engine: 'spatial_alignment_cpp23',
        database: { configured: false, healthy: false },
        strategy_engine: { configured: true, healthy: true },
        upstream: { provider: 'openf1', base_url: 'https://api.openf1.org/v1' },
        cache_entries: 7,
        uptime_seconds: 120
      })
    );

    const health = await checkGatewayHealth();
    expect(health.online).toBe(true);
    expect(health.database?.healthy).toBe(false);
    expect(health.strategyEngine?.healthy).toBe(true);
    expect(health.cacheEntries).toBe(7);
  });

  it('deixa o abort do chamador subir intacto, sem virar erro de gateway', async () => {
    const controller = new AbortController();
    controller.abort();
    vi.mocked(fetch).mockRejectedValue(new DOMException('Aborted', 'AbortError'));

    const failure = await fetchSessions(2024, controller.signal).catch((error: unknown) => error);
    expect(failure).not.toBeInstanceOf(ApiClientError);
    expect((failure as Error).name).toBe('AbortError');
  });
});
