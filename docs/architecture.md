# Arquitetura

## Princípio único

**Todo número exibido veio de uma medição.** Quando não há medição, o sistema diz
que não há — em vez de produzir um valor plausível. Esse princípio determina a
forma de cada camada abaixo e está registrado em
[ADR-006](adr/ADR-006-no-synthetic-telemetry.md).

---

## Fluxo de dados

```
                  ┌──────────────────────────────────────────┐
                  │          OpenF1 (api.openf1.org)         │
                  │  sessions · drivers · laps · stints      │
                  │  car_data · location · race_control      │
                  │  weather                                 │
                  └───────────────┬──────────────────────────┘
                                  │ HTTPS
              ┌───────────────────┴───────────────────┐
              │                                       │
   ┌──────────▼───────────┐              ┌────────────▼─────────────┐
   │   apex_ingest        │              │   api-gateway-cpp        │
   │   (C++23, em lote)   │              │   (C++23, serviço)       │
   │                      │              │                          │
   │ • arquiva o payload  │              │ 1. PostgreSQL            │
   │   bruto por SHA-256  │──escreve──┐  │ 2. OpenF1 ao vivo+cache  │
   │ • upsert idempotente │           │  │ 3. erro estruturado      │
   │ • COPY para séries   │           │  │                          │
   └──────────────────────┘           │  │ ┌──────────────────────┐ │
                                      │  │ │ analytics-cpp        │ │
              ┌───────────────────────▼┐ │ │ integra distância    │ │
              │ PostgreSQL/TimescaleDB ├─┼─┤ grade métrica        │ │
              │ sessions · drivers     │ │ │ delta · curvas       │ │
              │ laps · stints          │ │ │ microsetores · traps │ │
              │ telemetry_samples (HT) │ │ │ auditoria            │ │
              │ location_samples (HT)  │ │ └──────────┬───────────┘ │
              │ race_control · weather │ │            │             │
              │ raw_payloads           │ │  evidência │ medida      │
              └────────────────────────┘ │            ▼             │
                                         │ ┌──────────────────────┐ │
                                         │ │ strategy-hs (Haskell)│ │
                                         │ │ narrativa explicável │ │
                                         │ │ degradação de pneus  │ │
                                         │ └──────────────────────┘ │
                                         └────────────┬─────────────┘
                                                      │ REST + SSE
                                         ┌────────────▼─────────────┐
                                         │      apps/web (Next.js)  │
                                         │  gráficos · mapa · abas  │
                                         └──────────────────────────┘
```

---

## Serviços

### `services/common` — camada compartilhada

Existe para que gateway e ingestão falem com a OpenF1 pelo **mesmo** código. Antes
havia duas implementações do cliente HTTP e duas do parser, que divergiam.

- `http/` — cliente com handle persistente (keep-alive), rate limiting cooperativo
  e backoff exponencial. Recuo agressivo em HTTP 429.
- `openf1/` — cliente tipado da OpenF1 sobre simdjson. Normaliza as idiossincrasias
  do upstream: freio 0/1 versus 0/100, estados de DRS (8 = elegível, 10/12/14 =
  aberto), amostras de posição em (0,0) por perda de sinal, campos nulos que
  permanecem ausentes em vez de virarem zero.
- `time/` — ISO-8601 ↔ microssegundos UTC. Tempo absoluto nunca trafega em ponto
  flutuante.
- `hash/` — SHA-256 autocontido, para endereçar payloads por conteúdo.
- `testing/` — servidor HTTP mínimo, para exercitar o cliente pelo caminho real.

### `services/ingest-cpp` — ingestão

Processo em lote. Para cada sessão: arquiva cada resposta byte a byte endereçada
pelo digest, e escreve o resultado interpretado no armazém em transações
idempotentes. As séries de alta frequência entram por `COPY` em tabela temporária
promovida com `ON CONFLICT DO NOTHING` — milhares de amostras por volta tornariam
`INSERT` linha a linha inviável.

A cobertura de cada volta é **medida** (fração do tempo de volta coberta por
amostras, descontadas as lacunas) e gravada em `laps.coverage_pct`.

### `services/analytics-cpp` — motor de alinhamento espacial

Biblioteca ligada ao gateway. Puro cálculo, sem I/O:

1. **Integração de distância** — regra trapezoidal sobre velocidade e tempo.
2. **Normalização do eixo** — ver [ADR-007](adr/ADR-007-lap-axis-normalisation.md).
3. **Reamostragem** em grade métrica, com limite de lacuna derivado da taxa de
   amostragem real.
4. **Delta** `ΔT(d) = t_comp(d) − t_ref(d)`.
5. **Detecção de curvas** — mínimos locais de velocidade com proeminência mínima.
   Os rótulos são `C1..Cn`, a ordem de aparição, **não** a numeração oficial do
   circuito: a 4 Hz o traço não resolve curvas tomadas em carga plena, e rotulá-las
   como "T7" afirmaria uma correspondência que o dado não sustenta.
6. **Segmentos de perda** — janelas ancoradas nas curvas e nas retas entre elas,
   ordenadas por tempo perdido, com a causa dominante escolhida por peso relativo
   da evidência (velocidade de ápice, retomada de acelerador, ponto de freada,
   velocidade de ponta).
7. **Speed traps** — os sensores oficiais I1/I2/linha de chegada, localizados no
   eixo pelo tempo de setor real, mais os picos medidos nos canais.
8. **Auditoria** — cobertura, maior lacuna, intervalo mediano e o desvio de
   fechamento do delta contra o cronômetro.

### `services/api-gateway-cpp` — gateway

- `telemetry_resolver` — a cascata PostgreSQL → OpenF1 → erro, com cache tipado
  por TTL. Uma comparação precisa de pilotos, voltas e stints das duas voltas; sem
  memorizar, o mesmo `/drivers` seria pedido várias vezes na mesma requisição e o
  upstream passaria a recusar.
- `database_repository` — leitura do armazém, tipada e em JSON de passagem.
- `strategy_client` — HTTP para o motor Haskell, com timeout curto: sua
  indisponibilidade degrada a narrativa, não a análise.
- `http_server` — servidor próprio com roteamento por padrão e suporte a SSE.
- `metrics_collector` — exposição Prometheus, alimentada por todas as rotas.

### `services/strategy-hs` — motor de domínio

Serviço HTTP (warp) puro em relação aos dados: não busca telemetria e não guarda
estado. Recebe evidência já medida e devolve narrativa.

- `POST /v1/insights` — transforma segmentos em explicação. Cada frase cita um
  número da entrada. As premissas e limitações são **derivadas dos fatos**: compostos
  diferentes entre as voltas, diferença de idade de pneu, confiança baixa no trecho
  e perda próxima da resolução da grade viram itens declarados.
- `POST /v1/degradation` — regressão linear sobre os tempos reais do stint, com
  outliers de tráfego descartados, publicada ao lado da previsão do modelo térmico
  para que a diferença entre medição e modelo fique visível.

O tipo de retorno da recomendação de parada é `Maybe`: quando a degradação medida
não justifica a passagem pelos boxes dentro do horizonte restante, não existe
recomendação — e o sistema de tipos garante que o chamador trate esse caso.

### `apps/web` — interface

Cliente do gateway. Sem estado inicial de demonstração: cada recurso remoto tem
`data | loading | error`, e a interface distingue os três visualmente. O mapa do
circuito consome a geometria reconstruída pelo gateway — não há nenhuma coordenada
de circuito no frontend.

---

## Decisões registradas

| ADR | Assunto |
| --- | --- |
| [001](adr/ADR-001-cpp-package-manager.md) | Gerenciamento de dependências C++ |
| [002](adr/ADR-002-distance-based-telemetry-alignment.md) | Alinhamento por distância |
| [003](adr/ADR-003-domain-modeling-in-haskell.md) | Modelagem de domínio em Haskell |
| [004](adr/ADR-004-fast-dense-plotting.md) | Plotagem densa |
| [005](adr/ADR-005-sse-for-live-updates.md) | SSE em vez de WebSocket |
| [006](adr/ADR-006-no-synthetic-telemetry.md) | **Proibição de telemetria sintética** |
| [007](adr/ADR-007-lap-axis-normalisation.md) | **Normalização do eixo espacial** |
