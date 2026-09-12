# ADR-007: Normalização do eixo espacial contra o cronômetro oficial

## Status
Aceito — estende [ADR-002](ADR-002-distance-based-telemetry-alignment.md)

## Contexto

A distância percorrida é obtida integrando a velocidade pela regra trapezoidal
(ADR-002). A telemetria de carro da OpenF1 chega a ~4 Hz, e a integração nessa
frequência acumula um **erro de escala** que difere entre pilotos: a volta pole de
Verstappen em Sakhir 2024 integrava 5 370 m enquanto a volta de Leclerc integrava
um comprimento diferente para a mesma pista.

A consequência era silenciosa e grave. Com os dois eixos de distância medindo
réguas diferentes, o delta acumulado no último nó da grade valia **0,060 s**
enquanto a diferença cronometrada oficial entre as duas voltas era **0,301 s**. O
gráfico de delta — o principal instrumento da tela — estava errado por um fator de
cinco, e nada na interface indicava isso.

## Decisão

A volta de referência define o eixo espacial, e as duas voltas são recortadas
exatamente no instante cronometrado:

1. Descartar amostras posteriores a `lap_time_s`.
2. Fechar as duas pontas prolongando a última velocidade medida — a telemetria
   quase nunca começa exatamente no cruzamento da linha nem termina nele, e o
   trecho que falta é curto e foi percorrido em movimento. Extrapolação limitada a
   2 s; além disso o dado é genuinamente insuficiente.
3. Reancorar a origem do eixo na linha de chegada.
4. Reescalar o eixo da volta de comparação pelo fator $L_{\text{ref}} / L_{\text{comp}}$,
   de modo que as duas voltas meçam a mesma pista.

Com isso, em $d = L_{\text{ref}}$ cada volta marca o seu próprio tempo oficial e
portanto $\Delta T(L) = t_{\text{comp}} - t_{\text{ref}}$ por construção.

## Verificação

Como o resultado é verificável contra uma fonte independente — o cronômetro
oficial da FIA, que não participa do cálculo —, o desvio residual é **publicado**
em `quality_audit.delta_closure_error_s` e checado em testes:

- unitário, sobre um traçado sintético de resposta analítica conhecida
  (`analytics_test.cpp`: erro < 0,05 s);
- de integração, contra dados reais da OpenF1 (`scripts/verify-stack.mjs`:
  erro ≤ 0,02 s; medido em **0,0001 s** em Sakhir 2024 Q).

Um `delta_closure_error_s` que cresça é o sinal de que o eixo espacial deixou de
ser confiável, e a interface o exibe em cor de alerta acima de 0,02 s.

## Consequências

- `total_distance_m` passa a ser o comprimento **medido** na volta de referência
  (5 365 m em Sakhir, contra 5 412 m oficiais — 0,9% de desvio), e não um valor de
  catálogo. É uma medição, e é reportada como tal.
- Comparações entre sessões diferentes não compartilham eixo: o eixo é definido
  por volta de referência, dentro de uma comparação.
