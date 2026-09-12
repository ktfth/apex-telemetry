# ADR-006: Proibição de telemetria sintética e resolução em cascata

## Status
Aceito

## Contexto

Nas fases anteriores, cada camada do sistema tinha um caminho de reserva que
**gerava** dados quando a origem real não respondia:

- o gateway montava a volta com um modelo cinemático (`generate_live_comparison`)
  com frenagens fixas nas curvas de Sakhir e devolvia 200 OK;
- o motor numérico devolvia um segmento de insight codificado à mão
  (`"Curva 4, 0.228 s perdidos"`), independentemente da telemetria recebida;
- o stream SSE emitia $280 + 35\sin(d/200)$ km/h a 2 Hz;
- o frontend, quando o gateway não respondia, sintetizava a volta inteira a partir
  de um mapa de circuito desenhado à mão e de zonas de frenagem inventadas;
- o serviço de ingestão tinha um `--offline` que escrevia quatro pilotos fixos e um
  `--replay` que reproduzia uma senoide.

Nenhum desses caminhos se identificava na interface de forma inequívoca. Um
analista olhando o gráfico de velocidade não tinha como distinguir uma volta real
de uma volta gerada — e o produto existe precisamente para sustentar afirmações
técnicas sobre voltas reais.

O problema não é o valor estar errado por pouco. É que **um número fabricado
apresentado com a mesma autoridade visual de um número medido destrói o valor de
todos os outros números da tela.**

## Decisão

**Nenhuma camada sintetiza telemetria. Em nenhuma circunstância.**

A resolução de dados é uma cascata de origens reais, e o fim da cascata é um erro:

1. **PostgreSQL/TimescaleDB** — o armazém alimentado pelo serviço de ingestão.
2. **OpenF1 ao vivo** — a API pública, consultada pelo gateway e memorizada em
   cache com TTL. Isto é uma integração real, não um fallback fictício.
3. **Erro estruturado** — `{"error": ..., "code": ...}` com um código estável e o
   status HTTP correto.

Corolários obrigatórios:

- Toda resposta declara sua origem em `X-Apex-Data-Source`.
- Um campo ausente na origem é `null`, nunca um valor plausível. Uma volta sem
  tempo cronometrado tem `lap_time_s: null`, não `0`. A cobertura de uma volta
  cuja telemetria ainda não foi medida é `null`, não `100`.
- Uma recusa do upstream (HTTP 429/5xx) é `429`/`502`, nunca `404`: "não consegui
  perguntar" e "perguntei e não existe" levam o cliente a decisões diferentes.
- O frontend não tem estado inicial de demonstração. Ele distingue visualmente
  *carregando*, *sem dados* e *erro* — e um gráfico vazio com o motivo escrito é
  preferível a um gráfico preenchido.
- Textos explicativos só afirmam o que os números de entrada sustentam. A regra que
  dizia "apesar de velocidade de ápice equivalente" passou a verificar se a
  diferença é de fato menor que a resolução da medição antes de chamá-la equivalente.

## Consequências

- Sem gateway, a interface não funciona. Isso é correto: ela é um cliente de
  análise, não um catálogo de imagens.
- Os erros passam a fazer parte do contrato público e são testados
  (`scripts/verify-stack.mjs`, seção 7).
- O código sintético removido — modelos cinemáticos, registro de circuitos
  desenhados à mão, geradores procedurais, o modo `--offline` — deixou de existir
  em vez de ficar atrás de uma flag. Uma flag que gera dados acaba ligada.
