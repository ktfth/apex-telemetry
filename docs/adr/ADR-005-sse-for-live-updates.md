# ADR-005: SSE para Live Updates em vez de WebSocket

## Status
Aceito

## Contexto
A Fase 5 requer entrega de atualizações em tempo real ao dashboard (laps, race control, flags) durante sessões ativas de F1. Duas opções foram avaliadas:

1. **WebSocket**: bidirectional, full-duplex, requer estado persistente no servidor e handshake upgrade.
2. **Server-Sent Events (SSE)**: unidirectional (server→client), usa HTTP/1.1 padrão, reconexão automática nativa no browser (EventSource), sem necessidade de biblioteca adicional.

## Decisão
Adotar **SSE (Server-Sent Events)** para live updates no ApexTelemetry.

## Justificativa

1. **Unidirecionalidade natural**: O caso de uso é server→client (dados de telemetria, flags, laps). O cliente nunca envia dados em tempo real ao servidor.
2. **Reconexão automática**: A API `EventSource` do browser reconecta automaticamente com `Last-Event-ID`, eliminando lógica de retry manual.
3. **Simplicidade no C++23**: SSE requer apenas manter o socket aberto e escrever `data: ...\n\n`. Não há necessidade de frame encoding (WebSocket RFC 6455), masking, ou handshake upgrade.
4. **Compatibilidade HTTP**: SSE funciona com proxies, load balancers e CDNs padrão sem configuração especial.
5. **Fallback trivial**: Se SSE falhar, o dashboard simplesmente faz polling periódico dos endpoints REST existentes.

## Consequências
- O Gateway precisa suportar conexões long-lived (keep-alive) com flush line-by-line.
- Limite de 6 conexões SSE por origem no HTTP/1.1 (suficiente para o caso de uso single-tab).
- Se no futuro o cliente precisar enviar dados em tempo real (e.g., anotações colaborativas), WebSocket será reavaliado.
