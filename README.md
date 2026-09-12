# ApexTelemetry

Plataforma de análise de telemetria de Fórmula 1 alinhada por distância, sobre
dados reais da [OpenF1](https://openf1.org).

Compara duas voltas no mesmo ponto geométrico do traçado, quantifica onde o tempo
foi perdido e explica por quê — citando os números medidos que sustentam cada
afirmação.

---

## O princípio

**Todo número exibido veio de uma medição.** Quando não há medição, a plataforma
diz que não há.

Isso não é uma preferência de estilo. Um valor fabricado apresentado com a mesma
autoridade visual de um valor medido destrói a credibilidade de todos os outros
valores da tela — e a plataforma existe para sustentar afirmações técnicas sobre
voltas reais. Está registrado em
[ADR-006](docs/adr/ADR-006-no-synthetic-telemetry.md).

Em termos práticos:

- A resolução de dados é uma cascata de origens reais — PostgreSQL, depois OpenF1
  ao vivo — e termina em **erro estruturado**, nunca em dado gerado.
- Toda resposta declara sua origem em `X-Apex-Data-Source`.
- Campo ausente é `null`, nunca um valor plausível. Volta sem tempo cronometrado
  tem `lap_time_s: null`, não `0`.
- O mapa do circuito é reconstruído do transponder de posição. Não há nenhuma
  coordenada de circuito cadastrada em lugar nenhum do código.
- As curvas detectadas são rotuladas `C1..Cn` — a ordem em que aparecem na volta —
  e não `T1..Tn`, porque a amostragem disponível não resolve a numeração oficial.

---

## O alinhamento é verificável

A distância percorrida é obtida integrando a velocidade. A ~4 Hz, essa integração
acumula um erro de escala que **difere entre pilotos**: dois carros medem réguas
ligeiramente diferentes para a mesma pista. Sem corrigir isso, o gráfico de delta
— o principal instrumento da tela — erra por um fator de cinco.

A correção ([ADR-007](docs/adr/ADR-007-lap-axis-normalisation.md)) recorta as duas
voltas no instante cronometrado e reescala o eixo da volta de comparação para o
comprimento medido na referência. Por construção, o delta acumulado no fim da volta
passa a reproduzir a diferença cronometrada oficial.

Como o cronômetro da FIA não participa do cálculo, o desvio residual é uma
**verificação independente** — e por isso é publicado em
`quality_audit.delta_closure_error_s`, checado nos testes e exibido na interface.

> Bahrain 2024, Q — VER L16 (1:29.179) vs LEC L15 (1:29.480).
> Diferença oficial **0,301 s**; delta acumulado **0,301 s**;
> desvio de fechamento **0,0000 s** sobre 1 075 nós de 5 m.

---

## Estrutura

```
apps/web/              Next.js 15 — gráficos sincronizados, mapa, insights
services/
  common/              Cliente OpenF1, HTTP, tempo UTC, SHA-256 (C++23)
  ingest-cpp/          Ingestão idempotente + arquivo bruto por conteúdo
  analytics-cpp/       Motor de alinhamento espacial
  api-gateway-cpp/     REST + SSE; resolve PostgreSQL → OpenF1 → erro
  strategy-hs/         Motor de domínio em Haskell (explicações, degradação)
packages/
  contracts/           OpenAPI 3.1 → JSON Schema → tipos TypeScript
  ui/                  Design system de instrumentação
infra/                 TimescaleDB, Prometheus, Grafana
scripts/               verify-stack.mjs — verificação contra dados reais
docs/                  Arquitetura, contratos, guia de execução, ADRs
```

---

## Execução rápida

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j"$(nproc)"
(cd services/strategy-hs && cabal build all)
pnpm install

(cd services/strategy-hs && APEX_STRATEGY_PORT=8092 "$(cabal list-bin strategy-hs)") &
APEX_STRATEGY_URL=http://127.0.0.1:8092 ./build/services/api-gateway-cpp/apex_api_gateway 8080 &
pnpm dev
```

<http://localhost:3000>. Sem banco configurado, o gateway consulta a OpenF1 ao
vivo — a plataforma funciona de imediato, sem seed nem dados de exemplo.

Guia completo, ingestão para PostgreSQL e observabilidade:
[docs/quickstart.md](docs/quickstart.md).

---

## Capacidades

**Análise**

- Comparação de duas voltas em grade métrica configurável (1–50 m), com delta
  acumulado auditável contra o cronômetro oficial.
- Detecção de zonas lentas, microsetores de 100 m e speed traps, combinando os
  sensores oficiais de passagem com os picos medidos nos canais.
- Segmentos de perda ranqueados por tempo perdido, com a causa dominante escolhida
  por peso relativo da evidência — velocidade de ápice, retomada de acelerador,
  ponto de freada ou velocidade de ponta.
- Traçado do circuito reconstruído do transponder, com os ápices detectados
  posicionados sobre ele.
- Exportação em **MoTeC CSV** e JSON, com metadados reais da sessão.

**Explicabilidade (Haskell)**

- Narrativa em que cada frase cita um número medido.
- Premissas e limitações derivadas dos fatos: compostos diferentes entre as voltas,
  diferença de idade de pneu, confiança baixa no trecho e perda próxima da
  resolução da grade viram itens declarados.
- Degradação por stint: regressão sobre os tempos reais, com outliers de tráfego
  descartados, publicada ao lado da previsão do modelo térmico para que a diferença
  entre medição e modelo fique visível.

**Operação**

- Ingestão idempotente com arquivo bruto endereçado por SHA-256 e `COPY` em massa
  para as séries de alta frequência.
- Métricas Prometheus com latência por rota, origem efetiva de cada resposta e
  tráfego/recusas do upstream.
- Replay da volta em tempo de pista via SSE, quadro a quadro, com dados medidos.

---

## Testes

| Suíte | Comando | Escopo |
| --- | --- | --- |
| C++23 | `(cd build && ctest)` | 3 binários — integração de distância, normalização do eixo, detecção de curvas, JSON, MoTeC, SHA-256, ISO-8601, parsing da OpenF1 contra payloads reais servidos por HTTP |
| Haskell | `(cd services/strategy-hs && cabal test)` | 15 propriedades QuickCheck sobre o domínio |
| TypeScript | `pnpm test` | 18 testes — contrato do cliente, máquina de estados da store |
| Pilha real | `node scripts/verify-stack.mjs` | 53 verificações contra a OpenF1 ao vivo |

---

## Licença e dados

Os dados vêm da API pública [OpenF1](https://openf1.org), que os disponibiliza sob
seus próprios termos. Este projeto não redistribui dados da F1: ele os consulta em
tempo de execução e, opcionalmente, os arquiva localmente para reprodutibilidade.
