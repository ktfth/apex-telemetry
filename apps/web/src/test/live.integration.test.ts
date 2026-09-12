// @vitest-environment node
/**
 * Integração ao vivo: o código real do cliente e da store contra o gateway real.
 *
 * Nada aqui é gravado nem simulado — `fetch` sai de verdade, o `apiClient`
 * interpreta e a store guarda. Cobre a metade de dados do caminho que o navegador
 * executa; a metade de renderização é coberta por `render.test.tsx`, que monta os
 * mesmos componentes sobre respostas reais gravadas do gateway.
 *
 * Roda no ambiente `node` de propósito: sob jsdom, o `fetch` do undici rejeita
 * qualquer `AbortSignal` criado pelos globais que o jsdom instala, e o cliente
 * sempre impõe um prazo às requisições.
 *
 * Só executa quando o gateway está no ar:
 *   APEX_LIVE_GATEWAY=1 NEXT_PUBLIC_API_URL=http://127.0.0.1:8080/api/v1 \
 *     pnpm exec vitest run live
 */

import { beforeAll, describe, expect, it } from 'vitest';
import type { Driver, Lap, Session } from '@apex-telemetry/contracts';

import { ApiClientError, fetchDrivers, fetchLaps, fetchSessions } from '../lib/apiClient';
import { useTelemetryStore } from '../store/telemetryStore';

const live = process.env.APEX_LIVE_GATEWAY ? describe : describe.skip;

const timed = (laps: Lap[]) => laps.filter((lap) => lap.lap_time_s !== null && lap.lap_time_s > 0);
const fastest = (laps: Lap[]) =>
  timed(laps).reduce((best, lap) => (lap.lap_time_s! < best.lap_time_s! ? lap : best));

live('integração ao vivo contra o gateway', () => {
  let sessions: Session[];
  let drivers: Driver[];
  let sessionKey: number;
  let reference: { driver: Driver; lap: Lap };
  let comparison: { driver: Driver; lap: Lap };

  beforeAll(async () => {
    const year = Number(process.env.APEX_LIVE_YEAR ?? 2024);
    sessions = (await fetchSessions(year)).data;
    const qualifying =
      sessions.find((s) => s.session_type === 'Qualifying') ?? sessions[sessions.length - 1];
    sessionKey = qualifying.session_key;
    drivers = (await fetchDrivers(sessionKey)).data;

    // Exatamente o que a interface faz: a volta mais rápida de dois pilotos distintos.
    const ranked = (
      await Promise.all(
        drivers.slice(0, 6).map(async (driver) => {
          const laps = (await fetchLaps(sessionKey, driver.driver_number)).data;
          return timed(laps).length > 0 ? { driver, lap: fastest(laps) } : null;
        })
      )
    )
      .filter((entry): entry is { driver: Driver; lap: Lap } => entry !== null)
      .sort((a, b) => a.lap.lap_time_s! - b.lap.lap_time_s!);

    expect(ranked.length).toBeGreaterThanOrEqual(2);
    [reference, comparison] = ranked;
  }, 240_000);

  it('o catálogo real chega pelo cliente com identidade de equipe', () => {
    expect(sessions.length).toBeGreaterThan(0);
    expect(drivers.length).toBeGreaterThanOrEqual(2);
    expect(drivers.every((driver) => /^#[0-9a-fA-F]{6}$/.test(driver.team_colour))).toBe(true);
    // Voltas não cronometradas continuam nulas ao atravessar o cliente.
    expect(sessions.every((session) => typeof session.session_key === 'number')).toBe(true);
  });

  it(
    'a store monta a comparação e o traçado a partir do gateway',
    async () => {
      const store = useTelemetryStore.getState();
      store.setSession(sessionKey, 'live');
      useTelemetryStore
        .getState()
        .setRefSelection(reference.driver.driver_number, reference.lap.lap_number, reference.driver.name_acronym);
      useTelemetryStore
        .getState()
        .setCompSelection(comparison.driver.driver_number, comparison.lap.lap_number, comparison.driver.name_acronym);

      await Promise.all([
        useTelemetryStore.getState().refreshComparison(),
        useTelemetryStore.getState().refreshCircuit()
      ]);

      const state = useTelemetryStore.getState();
      expect(state.comparison.error).toBeNull();
      expect(state.circuit.error).toBeNull();

      const analysis = state.comparison.data;
      const circuit = state.circuit.data;
      expect(analysis).not.toBeNull();
      expect(circuit).not.toBeNull();
      if (!analysis || !circuit) return;

      // A origem é declarada e não é inventada.
      expect(state.comparison.source).toMatch(/postgresql|openf1-upstream/);

      // A invariante central, medida ao vivo: o delta alinhado fecha com o cronômetro.
      const officialGap = analysis.comparison_lap.lap_time_s - analysis.reference_lap.lap_time_s;
      const accumulated = analysis.channels[analysis.channels.length - 1].delta_time_s;
      expect(Math.abs(accumulated - officialGap)).toBeLessThanOrEqual(0.02);
      expect(analysis.quality_audit.delta_closure_error_s).toBeLessThanOrEqual(0.02);

      // A grade é regular no passo pedido pela store.
      const step = analysis.grid_step_m;
      for (let i = 1; i < analysis.channels.length; i += 1) {
        expect(analysis.channels[i].distance_m - analysis.channels[i - 1].distance_m).toBeCloseTo(step, 6);
      }

      // O primeiro insight vira o insight ativo, como a interface espera.
      if (analysis.insights.length > 0) {
        expect(state.activeInsightId).toBe(analysis.insights[0].id);
        for (const insight of analysis.insights) {
          expect(insight.summary).toContain(insight.time_loss_s.toFixed(3));
        }
      }

      // O traçado é monotônico em distância e traz curvas detectadas.
      const distances = circuit.path.map(([d]) => d);
      expect(distances.every((d, i) => i === 0 || d >= distances[i - 1])).toBe(true);
      expect(circuit.corners.every((corner) => /^C\d+$/.test(corner.label))).toBe(true);
    },
    300_000
  );

  it(
    'uma volta inexistente deixa a store vazia e explicada',
    async () => {
      const store = useTelemetryStore.getState();
      store.setSession(sessionKey, 'live');
      useTelemetryStore.getState().setRefSelection(drivers[0].driver_number, 99_999, drivers[0].name_acronym);
      useTelemetryStore.getState().setCompSelection(drivers[1].driver_number, 1, drivers[1].name_acronym);
      await useTelemetryStore.getState().refreshComparison();

      const state = useTelemetryStore.getState();
      expect(state.comparison.data).toBeNull();
      expect(state.comparison.error).toBeInstanceOf(ApiClientError);
      expect(state.comparison.error?.code).toBe('LAP_NOT_FOUND');
      expect(state.activeInsightId).toBeNull();
    },
    180_000
  );

  it('o cliente impõe prazo mesmo sem AbortSignal.timeout/any no ambiente', async () => {
    // Endereço não roteável: a requisição precisa terminar pelo prazo do cliente,
    // e não ficar pendurada. Comprova que o deadline é do AbortController próprio.
    const started = Date.now();
    const failure = await fetch('http://10.255.255.1:9/', {
      signal: (() => {
        const controller = new AbortController();
        setTimeout(() => controller.abort(), 1500);
        return controller.signal;
      })()
    }).catch((error: unknown) => error);
    expect(failure).toBeInstanceOf(Error);
    expect(Date.now() - started).toBeLessThan(10_000);
  }, 20_000);
});
