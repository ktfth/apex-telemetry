'use client';

import { useEffect } from 'react';
import type { LiveTelemetryTick } from '@apex-telemetry/contracts';
import { liveStreamUrl } from '../lib/apiClient';
import { useTelemetryStore } from '../store/telemetryStore';

/**
 * Replay em tempo de pista via Server-Sent Events.
 *
 * O gateway transmite a comparação já alinhada, quadro a quadro, com o intervalo
 * real entre os pontos dividido pelo fator de velocidade. Cada quadro carrega
 * apenas valores medidos — não há interpolação nem animação sintética do lado do
 * cliente, de modo que o cursor no mapa e nos gráficos marca onde o carro
 * realmente estava naquele instante da volta.
 */
export function useLiveSession(): void {
  const replayActive = useTelemetryStore((state) => state.replayActive);
  const replaySpeed = useTelemetryStore((state) => state.replaySpeed);
  const sessionKey = useTelemetryStore((state) => state.sessionKey);
  const refDriverNumber = useTelemetryStore((state) => state.refDriverNumber);
  const refLapNumber = useTelemetryStore((state) => state.refLapNumber);
  const compDriverNumber = useTelemetryStore((state) => state.compDriverNumber);
  const compLapNumber = useTelemetryStore((state) => state.compLapNumber);

  useEffect(() => {
    const store = useTelemetryStore.getState();

    if (
      !replayActive ||
      sessionKey === null ||
      refDriverNumber === null ||
      refLapNumber === null ||
      compDriverNumber === null ||
      compLapNumber === null
    ) {
      store.setLiveStreaming(false);
      store.setLiveTick(null);
      return;
    }

    const url = liveStreamUrl(
      {
        sessionKey,
        refDriver: refDriverNumber,
        refLap: refLapNumber,
        compDriver: compDriverNumber,
        compLap: compLapNumber,
        stepM: 5.0
      },
      replaySpeed
    );

    let source: EventSource;
    try {
      source = new EventSource(url);
    } catch (error) {
      store.setLiveError(error instanceof Error ? error.message : String(error));
      store.setLiveStreaming(false);
      return;
    }

    const onInit = () => {
      useTelemetryStore.getState().setLiveError(null);
      useTelemetryStore.getState().setLiveStreaming(true);
    };

    const onTick = (event: MessageEvent<string>) => {
      try {
        useTelemetryStore.getState().setLiveTick(JSON.parse(event.data) as LiveTelemetryTick);
      } catch {
        // Um quadro corrompido não derruba o stream; o próximo chega em milissegundos.
      }
    };

    /**
     * O mesmo tipo de evento cobre dois casos: um `event: error` nomeado que o
     * gateway envia com um corpo JSON, e a falha de transporte que o próprio
     * EventSource dispara sem corpo. Distinguimos pelo `data`.
     */
    const onError = (event: Event) => {
      const data = (event as MessageEvent<string>).data;
      let message = 'Conexão com o stream de replay interrompida.';
      if (typeof data === 'string' && data.length > 0) {
        try {
          const payload = JSON.parse(data) as { error?: string; code?: string };
          message = payload.error ?? payload.code ?? message;
        } catch {
          message = data;
        }
      }
      const current = useTelemetryStore.getState();
      current.setLiveError(message);
      current.setLiveStreaming(false);
      // Sem fechar, o EventSource reconectaria e o replay reiniciaria em laço.
      source.close();
    };

    const onEnd = () => {
      useTelemetryStore.getState().setLiveStreaming(false);
      source.close();
    };

    source.addEventListener('init', onInit);
    source.addEventListener('telemetry_tick', onTick as EventListener);
    source.addEventListener('error', onError);
    source.addEventListener('end', onEnd);

    return () => {
      source.removeEventListener('init', onInit);
      source.removeEventListener('telemetry_tick', onTick as EventListener);
      source.removeEventListener('error', onError);
      source.removeEventListener('end', onEnd);
      source.close();
      const current = useTelemetryStore.getState();
      current.setLiveStreaming(false);
      current.setLiveTick(null);
    };
  }, [replayActive, replaySpeed, sessionKey, refDriverNumber, refLapNumber, compDriverNumber, compLapNumber]);
}
