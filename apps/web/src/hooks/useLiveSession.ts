'use client';

import { useEffect, useRef } from 'react';
import { useTelemetryStore, type LiveTelemetryTick } from '../store/telemetryStore';

const API_BASE_URL = process.env.NEXT_PUBLIC_API_URL || 'http://localhost:8080/api/v1';

export function useLiveSession(sessionKey: number, enabled: boolean = true) {
  const { setLiveStreaming, setLiveTick } = useTelemetryStore();
  const eventSourceRef = useRef<EventSource | null>(null);

  useEffect(() => {
    if (!enabled) {
      setLiveStreaming(false);
      return;
    }

    const url = `${API_BASE_URL}/sessions/${sessionKey}/live`;
    let es: EventSource | null = null;

    try {
      es = new EventSource(url);
      eventSourceRef.current = es;

      es.addEventListener('init', () => {
        setLiveStreaming(true);
      });

      es.addEventListener('telemetry_tick', (e) => {
        try {
          const tick: LiveTelemetryTick = JSON.parse(e.data);
          setLiveTick(tick);
          setLiveStreaming(true);
        } catch (err) {
          console.error('[SSE] Failed to parse telemetry tick', err);
        }
      });

      es.onerror = () => {
        setLiveStreaming(false);
        // O navegador tentará reconectar automaticamente pelo padrão SSE
      };
    } catch {
      setLiveStreaming(false);
    }

    return () => {
      if (es) {
        es.close();
        setLiveStreaming(false);
        setLiveTick(null);
      }
    };
  }, [sessionKey, enabled, setLiveStreaming, setLiveTick]);
}
