# ADR-004: Visualização de Séries Temporais Densas na Interface Web

## Status
Aceito

## Contexto
Uma volta típica de F1 tem duração aproximada de 75–100 segundos. A 10 Hz ou 20 Hz, são coletados de 750 a 2000 pontos por canal. Ao alinhar duas voltas a cada 5 metros numa pista de 5,4 km, temos 1.080 pontos por canal. Para 6 canais por volta (Speed, Throttle, Brake, RPM, Gear, DRS) e duas voltas simultâneas mais o canal de Delta, são mais de 14.000 pontos rendered por comparação.

Bibliotecas comuns baseadas em React SVG DOM (ex: Recharts) degradam significativamente a performance e bloqueiam a thread principal de renderização, inviabilizando interação suave a 60 fps durante o rastreamento do mouse (crosshair).

## Decisão
Adotamos uma arquitetura de visualização em camadas:
1. **uPlot / Canvas para Telemetria Multicanal**:
   - Desenho de gráficos de alta densidade no Canvas 2D via uPlot com overhead mínimo de memória.
   - Sincronização de crosshair entre múltiplos subgráficos com zero atraso percebido e consumo ínfimo de CPU.
2. **SVG Vetorial Paramétrico para o Mapa do Circuito**:
   - Geometria da pista construída via caminhos SVG normalizados com projeção ortogonal.
   - Segmentação de micro-setores codificada por gradiente de Delta ($\Delta T$) com marcadores interativos em tempo real sincronizados ao cursor da telemetria.
3. **Zustand para Estado Efêmero de Alta Frequência**:
   - Posição do cursor (`currentDistanceM`, `hoveredSample`) transmitida via Zustand fora da árvore reativa do React para evitar re-renderizações desnecessárias dos painéis estáticos.

## Consequências
- Interação fluida e sem stutter a 60 fps.
- Visual sóbrio e estritamente técnico de instrumentação de laboratório de pista.
