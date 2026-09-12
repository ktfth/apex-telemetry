/**
 * Testes de renderização contra respostas reais do gateway.
 *
 * Os payloads em `recorded/` foram capturados de Bahrain 2024 Q através do motor
 * de alinhamento espacial — não são valores inventados para o teste. O objetivo é
 * garantir que a interface exibe o que foi medido, e que os estados de carregando,
 * vazio e erro são visualmente distintos de um resultado válido.
 */

import { cleanup, render, screen, within } from '@testing-library/react';
import { afterEach, beforeEach, describe, expect, it } from 'vitest';
import type { CircuitGeometry, LapComparison } from '@apex-telemetry/contracts';

import recordedComparison from './recorded/lap-comparison.9468.VER16-LEC15.json';
import recordedCircuit from './recorded/circuit.9468.json';

import { ApiClientError } from '../lib/apiClient';
import { useTelemetryStore, type TelemetryState } from '../store/telemetryStore';
import { TelemetryPlots } from '../components/TelemetryPlots';
import { InsightsPanel } from '../components/InsightsPanel';
import { CircuitMap } from '../components/CircuitMap';

const comparison = recordedComparison as unknown as LapComparison;
const circuit = recordedCircuit as unknown as CircuitGeometry;

/** Aplica um recorte do estado; `setState` do zustand aceita parcial sem `replace`. */
function setState(partial: Partial<TelemetryState>) {
  useTelemetryStore.setState(partial as TelemetryState);
}

function loadRecorded() {
  setState({
    sessionKey: comparison.session_key,
    sessionLabel: `${comparison.country_name} · ${comparison.session_name}`,
    refDriverNumber: comparison.reference_lap.driver_number,
    refDriverCode: comparison.reference_lap.driver_code,
    refLapNumber: comparison.reference_lap.lap_number,
    compDriverNumber: comparison.comparison_lap.driver_number,
    compDriverCode: comparison.comparison_lap.driver_code,
    compLapNumber: comparison.comparison_lap.lap_number,
    comparison: { data: comparison, loading: false, error: null, source: 'openf1-upstream' },
    circuit: { data: circuit, loading: false, error: null, source: 'openf1-upstream' },
    activeInsightId: comparison.insights[0]?.id ?? null,
    hoveredDistanceM: null,
    insightTab: 'insights'
  });
}

function reset() {
  setState({
    comparison: { data: null, loading: false, error: null, source: null },
    circuit: { data: null, loading: false, error: null, source: null },
    hoveredDistanceM: null,
    activeInsightId: null,
    insightTab: 'insights'
  });
}

describe('renderização com dados reais gravados', () => {
  beforeEach(reset);
  afterEach(cleanup);

  it('o payload gravado é de fato uma volta real de Bahrain 2024', () => {
    expect(comparison.circuit_name).toBe('Sakhir');
    expect(comparison.reference_lap.lap_time_s).toBeCloseTo(89.179, 3);
    expect(comparison.comparison_lap.lap_time_s).toBeCloseTo(89.48, 3);
    expect(comparison.data_source).not.toBe('unknown');
    // A invariante central: o delta alinhado reproduz o cronômetro oficial.
    expect(comparison.quality_audit.delta_closure_error_s).toBeLessThanOrEqual(0.02);
  });

  it('exibe os canais medidos na barra de instrumentos', () => {
    loadRecorded();
    const target = comparison.channels[10];
    setState({ hoveredDistanceM: target.distance_m });

    render(<TelemetryPlots />);

    /** Localiza o cartão de um instrumento pelo rótulo, para ler os dois valores. */
    const instrument = (label: string) => {
      const title = screen.getByText(label);
      const card = title.closest('div')?.parentElement;
      if (!card) throw new Error(`instrumento "${label}" não encontrado`);
      return within(card as HTMLElement);
    };

    expect(instrument('Distância').getByText(String(Math.round(target.distance_m)))).toBeTruthy();

    const speed = instrument('Velocidade');
    expect(speed.getByText(target.ref.speed_kmh.toFixed(1))).toBeTruthy();
    expect(speed.getByText(target.comp.speed_kmh.toFixed(1))).toBeTruthy();

    // As duas voltas aparecem mesmo quando coincidem — o instrumento não deduplica.
    const gear = instrument('Marcha');
    expect(gear.getAllByText(`G${target.ref.gear}`).length).toBeGreaterThanOrEqual(1);
    expect(gear.getByText(comparison.comparison_lap.driver_code)).toBeTruthy();

    // O sinal do delta indica quem está à frente, e isso precisa aparecer.
    const delta = `${target.delta_time_s >= 0 ? '+' : ''}${target.delta_time_s.toFixed(3)}`;
    expect(instrument('Delta').getByText(delta)).toBeTruthy();
  });

  it('desenha uma curva por canal, com um nó por ponto medido', () => {
    loadRecorded();
    render(<TelemetryPlots />);

    const speedChart = screen.getByTestId('chart-speed');
    const traces = [...speedChart.querySelectorAll('path')];
    expect(traces.length).toBe(2); // referência e comparação

    for (const trace of traces) {
      const nodes = (trace.getAttribute('d') ?? '').split(/[ML]/).filter(Boolean);
      expect(nodes.length).toBe(comparison.channels.length);
    }

    // Pedais: acelerador e freio das duas voltas.
    expect(screen.getByTestId('chart-pedals').querySelectorAll('path').length).toBe(4);
  });

  it('usa as cores reais das equipes, e não uma paleta fixa', () => {
    loadRecorded();
    render(<TelemetryPlots />);
    const strokes = [...screen.getByTestId('chart-speed').querySelectorAll('path')].map((p) =>
      p.getAttribute('stroke')
    );
    expect(strokes).toContain(comparison.reference_lap.team_colour);
    expect(strokes).toContain(comparison.comparison_lap.team_colour);
  });

  it('mostra os insights com a perda medida e a evidência que a sustenta', () => {
    loadRecorded();
    render(<InsightsPanel />);

    for (const insight of comparison.insights) {
      expect(screen.getByText(`+${insight.time_loss_s.toFixed(3)} s`)).toBeTruthy();
      expect(screen.getByText(insight.summary)).toBeTruthy();
    }
    // Premissas e limitações declaradas pelo motor chegam à tela.
    for (const limitation of comparison.insights.flatMap((i) => i.limitations)) {
      expect(screen.getByText(limitation)).toBeTruthy();
    }
  });

  it('publica a auditoria de qualidade, incluindo o fechamento do delta', () => {
    loadRecorded();
    render(<InsightsPanel />);
    expect(screen.getByText(comparison.quality_audit.delta_closure_error_s.toFixed(4) + ' s')).toBeTruthy();
    expect(screen.getByText(`${comparison.quality_audit.coverage_pct.toFixed(1)}%`)).toBeTruthy();
    expect(screen.getByText(comparison.quality_audit.source_notes)).toBeTruthy();
  });

  it('distingue speed traps oficiais de picos medidos nos canais', () => {
    loadRecorded();
    setState({ insightTab: 'speed_traps' });
    render(<InsightsPanel />);

    const official = comparison.speed_traps.filter((t) => t.origin === 'openf1_marshalling_loop');
    const derived = comparison.speed_traps.filter((t) => t.origin === 'channel_peak');
    expect(official.length).toBeGreaterThan(0);
    expect(derived.length).toBeGreaterThan(0);
    expect(screen.getAllByText('sensor oficial de passagem').length).toBe(official.length);
    expect(screen.getAllByText('pico medido no canal de velocidade').length).toBe(derived.length);
  });

  it('desenha o traçado reconstruído e posiciona as curvas detectadas', () => {
    loadRecorded();
    render(<CircuitMap />);

    const track = screen.getByTestId('circuit-track');
    const trackPath = track.querySelector('path');
    expect(trackPath).toBeTruthy();
    const nodes = (trackPath!.getAttribute('d') ?? '').split(/[ML]/).filter(Boolean);
    expect(nodes.length).toBe(circuit.path.length);

    for (const corner of circuit.corners) {
      expect(screen.getByText(corner.label)).toBeTruthy();
    }
    expect(screen.getByText(circuit.circuit_name)).toBeTruthy();
  });

  it('o cursor espacial posiciona o carro sobre o traçado real', () => {
    loadRecorded();
    setState({ hoveredDistanceM: circuit.corners[0].apex_distance_m });
    render(<CircuitMap />);

    const markers = [...screen.getByTestId('circuit-track').querySelectorAll('circle')];
    // Uma marca por curva mais a posição do cursor.
    expect(markers.length).toBe(circuit.corners.length + 1);
    expect(screen.getByText(`${Math.round(circuit.corners[0].apex_distance_m)} m`)).toBeTruthy();
  });
});

describe('estados sem dados são visualmente distintos de um resultado', () => {
  beforeEach(reset);
  afterEach(cleanup);

  it('sem seleção, os gráficos pedem uma seleção em vez de desenhar uma volta', () => {
    render(<TelemetryPlots />);
    expect(screen.getByText(/Nenhuma comparação ativa/)).toBeTruthy();
    expect(screen.queryByTestId('chart-speed')).toBeNull();
    expect(screen.queryByTestId('chart-delta')).toBeNull();
  });

  it('carregando, os gráficos não desenham nada', () => {
    setState({ comparison: { data: null, loading: true, error: null, source: null } });
    render(<TelemetryPlots />);
    expect(screen.getByText(/alinhando/i)).toBeTruthy();
    expect(screen.queryByTestId('chart-speed')).toBeNull();
  });

  it('em erro, mostra o código e a mensagem do gateway — e nenhuma curva', () => {
    setState({
      comparison: {
        data: null,
        loading: false,
        error: new ApiClientError('Lap 99 has no recorded lap time', 'LAP_NOT_FOUND', 404),
        source: null
      }
    });
    render(<TelemetryPlots />);
    expect(screen.getByText('LAP_NOT_FOUND')).toBeTruthy();
    expect(screen.getByText('Lap 99 has no recorded lap time')).toBeTruthy();
    expect(screen.queryByTestId('chart-speed')).toBeNull();
    expect(screen.queryByTestId('chart-delta')).toBeNull();
    expect(screen.queryByTestId('chart-pedals')).toBeNull();
  });

  it('o mapa em erro não desenha um traçado plausível', () => {
    setState({
      circuit: {
        data: null,
        loading: false,
        error: new ApiClientError('Sem posições de transponder', 'TRACK_GEOMETRY_UNAVAILABLE', 404),
        source: null
      }
    });
    render(<CircuitMap />);
    expect(screen.getByText('TRACK_GEOMETRY_UNAVAILABLE')).toBeTruthy();
    expect(screen.queryByTestId('circuit-track')).toBeNull();
  });
});
