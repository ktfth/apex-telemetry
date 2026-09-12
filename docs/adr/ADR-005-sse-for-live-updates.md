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
5. **Falha visível**: se o stream cair, o cliente fecha o `EventSource`, encerra o replay e exibe o motivo. Não há polling silencioso de reposição: um replay que continua sem dados novos é indistinguível de um replay correto, e essa ambiguidade é pior do que a interrupção.

## Consequências
- O Gateway precisa suportar conexões long-lived (keep-alive) com flush linha a linha.
- Limite de 6 conexões SSE por origem no HTTP/1.1 (suficiente para o caso de uso de aba única).
- O `EventSource` reconecta sozinho por padrão. Como o stream é um **replay finito** de uma volta, e não um feed perpétuo, o cliente precisa fechar explicitamente a conexão ao receber `end` ou `error` — sem isso o replay reiniciaria em laço.

## Nota de implementação (2026)
O stream transporta a comparação já alinhada, quadro a quadro, com o intervalo entre
quadros igual ao tempo real entre os pontos medidos dividido pelo fator `speed`. Cada
quadro carrega apenas valores medidos. A implementação anterior emitia uma onda
senoidal gerada no servidor: era indistinguível de telemetria para quem olhasse o
gráfico, e por isso foi removida.
- Se no futuro o cliente precisar enviar dados em tempo real (e.g., anotações colaborativas), WebSocket será reavaliado.
