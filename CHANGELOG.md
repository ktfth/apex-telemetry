# Changelog

Formato baseado em [Keep a Changelog](https://keepachangelog.com/pt-BR/1.1.0/).

---

## [2.0.0] — 2026-09-12

Substituição de toda telemetria sintética por integração real com a
[OpenF1](https://openf1.org), e correção dos defeitos de exatidão que a
integração real expôs.

### O problema

Cada camada tinha um caminho que **gerava** dados quando a origem real não
respondia, e nenhum deles se identificava na interface:

- o gateway montava a volta com um modelo cinemático de Sakhir e devolvia `200 OK`;
- o motor numérico devolvia sempre o mesmo segmento codificado à mão
  (`"Curva 4, 0.228 s perdidos"`), independentemente da telemetria recebida;
- o stream SSE emitia `280 + 35·sin(d/200)` km/h a 2 Hz;
- o frontend sintetizava a volta inteira a partir de um mapa de circuito
  desenhado à mão e de zonas de frenagem inventadas;
- a ingestão tinha um `--offline` com quatro pilotos fixos e um `--replay`
  que reproduzia uma senoide.

Um analista olhando o gráfico de velocidade não tinha como distinguir uma volta
real de uma gerada. Ver [ADR-006](docs/adr/ADR-006-no-synthetic-telemetry.md).

### Corrigido

- **O gráfico de delta errava por um fator de cinco.** A integração de distância
  a 4 Hz acumula erro de escala *diferente entre pilotos* — dois carros mediam
  réguas diferentes para a mesma pista. O delta acumulado dava 0,060 s onde a
  diferença cronometrada era 0,301 s. O eixo passa a ser normalizado contra o
  cronômetro oficial, e o desvio residual é publicado em
  `quality_audit.delta_closure_error_s` como verificação independente.
  Ver [ADR-007](docs/adr/ADR-007-lap-axis-normalisation.md).
- **A cobertura reportava 80% sem haver lacuna alguma.** O limite fixo de 25 m
  classificava o espaçamento normal da amostragem a 4 Hz (~21 m a 320 km/h)
  como falha de dados. O limite passa a ser derivado da cadência real.
- **Recusa do upstream virava `404`.** HTTP 429/5xx da OpenF1 eram reportados
  como "não existe", fazendo o cliente desistir de um dado que existe. Agora
  `429`/`502` são distintos de `404`.
- **Curvas rotuladas com a numeração oficial do circuito.** A amostragem
  disponível resolve 6 a 7 zonas lentas em Sakhir, que tem 15 curvas; chamá-las
  de `T1..T7` afirmava uma correspondência que o dado não sustenta. Passam a ser
  `C1..Cn`, a ordem de aparição na volta.
- **Narrativa contradizia os próprios números.** A frase "apesar de velocidade
  de ápice equivalente" saía com 5 km/h de diferença, e falava em "ápice" em
  trechos de reta. Passa a verificar se a diferença está dentro da resolução da
  medição antes de chamá-la equivalente.
- **`AbortSignal.any` ausente deixava a requisição sem prazo.** A detecção de
  suporte devolvia apenas o sinal do chamador quando o estático faltava — sem
  prazo algum justamente no navegador mais antigo. Reescrito sobre
  `AbortController`, com o temporizador cancelado quando a resposta chega.
- **Seletor de ano incoerente.** A store abria no ano corrente mas a lista era
  fixa em `[2025, 2024, 2023]`. A lista passa a ser derivada do ano corrente.
- **Seleção inicial inútil.** O padrão pegava os dois primeiros pilotos por
  número; em Melbourne 2026 isso comparava uma pole com uma volta de instalação
  (22 s de delta). Passa a usar os dois mais rápidos da sessão.
- **Logs vazios.** `std::cout` redirecionado para arquivo fica totalmente
  bufferizado: o log só apareceria quando o processo morresse. Corrigido com
  `unitbuf` no gateway e `LineBuffering` no motor Haskell, mais um log de acesso
  com status, rota, duração, origem e corpo do erro em 4xx.

### Adicionado

- `services/common`: cliente OpenF1 tipado, cliente HTTP com keep-alive e
  backoff, tempo UTC em microssegundos e SHA-256 — compartilhados entre gateway
  e ingestão, que antes tinham implementações divergentes.
- Endpoints `/sessions/{key}/stints`, `/weather`, `/circuit` e
  `/analysis/degradation`.
- Traçado do circuito reconstruído das amostras de posição do transponder.
  Nenhuma coordenada de circuito existe no código.
- `strategy-hs` passa a ser serviço HTTP (warp + aeson) consumido pelo gateway,
  com narrativa derivada da evidência medida e degradação por regressão sobre os
  tempos reais do stint, com outliers de tráfego descartados.
- Ingestão completa: voltas, stints, telemetria, posição, direção de prova e
  clima, com `COPY` em massa para as séries e arquivo bruto endereçado por
  SHA-256.
- Cache tipado no gateway: uma comparação passa de ~11 chamadas ao upstream para
  2 em cache quente. Trocar de volta cai de ~6 s para 295 ms; voltar a uma
  seleção já vista, para 9 ms.
- `scripts/verify-stack.mjs`: 53 verificações contra a OpenF1 ao vivo.
- Testes de renderização sobre respostas reais gravadas e teste de integração ao
  vivo do cliente contra o gateway.

### Removido

- Fixtures do frontend (`demoBahrain2024.ts`, `circuitRegistry.ts`).
- `telemetryMath.ts` — duplicata em TypeScript do motor de alinhamento em C++.
- Geradores sintéticos do gateway, modo `--offline` e replay senoidal da
  ingestão.
- Redis e NATS do `docker-compose`: nenhum serviço os usava.
- Executável `apex_analytics`, que apenas imprimia a própria versão. O motor é
  biblioteca ligada ao gateway.
- Métrica `apex_fallbacks_total` e o alvo de scraping do Prometheus que apontava
  para hosts inexistentes.

### Contratos

Esquema de comparação em **1.2.0**. `openapi.yaml` reescrito (12 rotas, 22
esquemas); o JSON Schema passa a ser gerado dele por `pnpm contracts:schema`.

### Verificação

| Suíte | Resultado |
| --- | --- |
| CTest (C++23) | 3/3 |
| QuickCheck (Haskell) | 15 propriedades |
| Vitest | 31 testes |
| Integração ao vivo | 4 testes |
| `verify-stack.mjs` | 53/53 contra a OpenF1 |

Bahrain 2024 Q — VER 1:29.179 vs LEC 1:29.480: diferença oficial 0,301 s, delta
acumulado 0,301 s, desvio de fechamento **0,0000 s** sobre 1.075 nós de 5 m.

### Limitações conhecidas

- **O caminho de escrita no PostgreSQL não foi exercitado contra um servidor
  real.** Foi construído e revisado, mas não há banco disponível no ambiente de
  desenvolvimento usado. O caminho OpenF1 está integralmente verificado.
- A primeira carga de uma sessão nova leva ~6 s: são as idas à OpenF1.
  Eliminá-las é o papel da ingestão para PostgreSQL.
- A cobertura de telemetria de carro da OpenF1 começa em 2023.

---

## [1.6.0] — Fases 1 a 6

Desenvolvimento inicial: monorepo, design system, aplicação Next.js, motor
numérico C++23, motor de domínio em Haskell, observabilidade e exportação MoTeC.
Nessas fases a plataforma ainda operava sobre telemetria gerada quando a origem
real não respondia — situação revertida em 2.0.0.
