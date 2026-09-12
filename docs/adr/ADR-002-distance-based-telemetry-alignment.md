# ADR-002: Alinhamento e Comparação Espacial por Distância

## Status
Aceito — estendido por [ADR-007](ADR-007-lap-axis-normalisation.md), que acrescenta a
normalização do eixo espacial necessária para o delta fechar contra o cronômetro oficial.

## Contexto
Na análise de telemetria de competição, comparar voltas sobre o eixo temporal ($t$) gera divergência progressiva: se o Piloto A freia 10 metros mais tarde ou é mais veloz na reta, o segundo piloto fica defasado no tempo, impossibilitando comparar o comportamento dinâmico (velocidade mínima de curva, ponto de reaceleração, ápice) no mesmo ponto geométrico do circuito.

## Decisão
Adotamos o **Eixo de Distância Cumulativa ($d$)** como a referência primária absoluta de alinhamento em todas as comparações de voltas:

1. **Integração Numérica de Distância**:
   Para amostras ordenadas temporalmente $(t_i, v_i)$:
   $$\Delta t_i = t_i - t_{i-1}$$
   $$v_{\text{mps}, i} = \frac{v_{i, \text{km/h}}}{3.6}$$
   $$d_i = d_{i-1} + \frac{v_{\text{mps}, i} + v_{\text{mps}, i-1}}{2} \cdot \Delta t_i$$

2. **Reamostragem em Grade Uniforme**:
   - Resolução base: **$\Delta d = 5.0\text{ metros}$**.
   - O limite de interpolação é **derivado da amostragem real**, e não fixo: $2{,}5 \times \Delta t_{\text{mediano}} \times v_{\max}$, com piso de 25 m. A telemetria de carro da OpenF1 chega a ~4 Hz, o que a 320 km/h significa ~21 m entre amostras; um limite fixo de 25 m classificaria o regime normal de amostragem como falha de dados e derrubaria a cobertura reportada para ~80% sem que houvesse qualquer lacuna real.
   - Acima desse limite o ponto é marcado `is_extrapolated`, entra no cálculo de cobertura e **não** é interpolado.

3. **Cálculo do Delta Cumulativo de Tempo ($\Delta T(d)$)**:
   - Para cada nó da grade $d_k$, recupera-se o tempo decorrido desde a linha de largada $T_A(d_k)$ e $T_B(d_k)$.
   - $\Delta T(d_k) = T_B(d_k) - T_A(d_k)$, com $A$ = volta de referência e $B$ = volta de comparação.
   - $\Delta T(d_k) > 0$ significa que a volta de comparação está mais lenta naquele ponto do traçado.

## Consequências
- Os gráficos de telemetria (Speed, Throttle, Brake, RPM, Gear, DRS, Time Delta) compartilham o mesmo domínio $[0, D_{\text{lap}}]$ em metros.
- O cursor (crosshair) aponta exatamente para a mesma coordenada espacial no mapa vetorial do circuito, permitindo correlação direta entre o traçado da curva e o comportamento dos pedais.
