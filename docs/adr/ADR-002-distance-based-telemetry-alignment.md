# ADR-002: Alinhamento e Comparação Espacial por Distância

## Status
Aceito

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
   - Interpolação linear/spline aplicada exclusivamente quando a distância entre amostras reais consecutivas for $\le 25.0\text{ metros}$ e $\Delta t \le 1.0\text{ s}$.
   - Se houver lacuna superior a 25 m, o trecho é classificado como `DISCONTINUOUS` e recebe flag de incerteza, sem extrapolação fictícia.

3. **Cálculo do Delta Cumulativo de Tempo ($\Delta T(d)$)**:
   - Para cada nó da grade $d_k$, recupera-se o tempo decorrido desde a linha de largada $T_A(d_k)$ e $T_B(d_k)$.
   - $\Delta T(d_k) = T_B(d_k) - T_A(d_k)$.
   - Um $\Delta T(d_k) > 0$ indica que o Piloto A está à frente (ganho de tempo em relação a B).

## Consequências
- Os gráficos de telemetria (Speed, Throttle, Brake, RPM, Gear, DRS, Time Delta) compartilham o mesmo domínio $[0, D_{\text{lap}}]$ em metros.
- O cursor (crosshair) aponta exatamente para a mesma coordenada espacial no mapa vetorial do circuito, permitindo correlação direta entre o traçado da curva e o comportamento dos pedais.
