# ApexTelemetry — Arquitetura do Sistema

## 1. Visão Geral

O **ApexTelemetry** é uma plataforma de engenharia para análise aprofundada de telemetria de Fórmula 1 voltada a engenheiros, analistas de performance e criadores de conteúdo técnico. O sistema foi desenhado segundo princípios de computação de alta performance e determinismo analítico:

1. **Alta Densidade e Baixa Latência (C++23)**: Ingestão, normalização, reamostragem espacial (distância) e cálculos numéricos pesados executados nativamente.
2. **Determinismo e Explicabilidade Formal (Haskell)**: Classificação de voltas, stints, estratégias e geração de insights auditáveis baseados exclusivamente em evidências empíricas e tipos estritos.
3. **Interface Técnica de Engenharia (Next.js / TypeScript / uPlot / SVG)**: Experiência visual sóbria estilo instrumentação de telemetria de pista, com crosshair sincronizado em 60fps, mapas de circuito vetoriais com delta heatmap e dados tabulares mono-espaçados.

```mermaid
flowchart TD
    subgraph DataSources["Fontes Externas"]
        OpenF1["OpenF1 API / WebSocket"]
        LiveTiming["Live Timing Stream (Futuro)"]
    end

    subgraph Storage["Camada de Persistência & Streaming"]
        Postgres[("PostgreSQL 16 + TimescaleDB\n(Raw + Normalized + Laps)")]
        Redis[("Redis 7\n(Sessões ativas & Cache efêmero)")]
        NATS[("NATS JetStream\n(Bus de eventos distribuídos)")]
    end

    subgraph Services["Serviços Especializados"]
        Ingest["ingest-cpp\n(C++23 Ingestion, Normalizer & Replay)"]
        Analytics["analytics-cpp\n(C++23 Numerical Distance Resampling & Delta)"]
        Strategy["strategy-hs\n(Haskell Pure Rules, Stints & Evidence Insights)"]
        Gateway["api-gateway-cpp\n(C++23 Drogon REST & WS)"]
    end

    subgraph Client["Camada de Apresentação"]
        WebUI["apps/web (Next.js App Router)\n- uPlot Synchronized Multi-Series\n- SVG Interactive Circuit Map\n- TanStack Query + Zustand\n- Tabular Monospace Instrumentation"]
    end

    OpenF1 --> Ingest
    LiveTiming --> Ingest
    Ingest -->|Raw Payloads & Normalized Events| Postgres
    Ingest -->|Telemetria Raw Streams| NATS
    NATS --> Analytics
    Postgres --> Analytics
    Analytics -->|Distance Resampled Series & Deltas| Postgres
    Analytics -->|Telemetry Segments & Metrics| Strategy
    Strategy -->|Validated Stints & Explanations| Postgres
    Gateway --> Postgres
    Gateway --> Redis
    Gateway --> Analytics
    Gateway --> Strategy
    WebUI <-->|HTTP / JSON REST & WS| Gateway
```

---

## 2. Fronteiras de Responsabilidade e Separação de Serviços

| Serviço | Linguagem / Stack | Papel Operacional | Justificativa Técnica |
| :--- | :--- | :--- | :--- |
| **`ingest-cpp`** | C++23, libcurl, simdjson, libpqxx, NATS C++ | Ingestão resiliente, retry exponencial, parse ultra-rápido de JSON, normalização e publicação em streaming. | Zero-allocation parsing via simdjson, throughput de dezenas de milhares de amostras/s, controle fino de memória. |
| **`analytics-cpp`** | C++23, Eigen / std::valarray, Drogon / Boost.Asio | Reamostragem espacial baseada em distância cumulativa ($\Delta d = v \cdot \Delta t$), interpolação spline/linear com salvaguardas, cálculo de delta temporal e alinhamento de canais. | Cálculos vetoriais paralelizáveis com vetores contíguos de memória em cache L1/L2, sem garbage collection. |
| **`strategy-hs`** | Haskell (GHC), Servant, Aeson, Hasql | Classificação categórica de voltas (`flying`, `out-lap`, `in-lap`, `safety-car`), stints de pneus, validação de integridade e geração de insights explicáveis. | O sistema de tipos algébricos e pureza funcional previne estados inválidos e garante regras auditáveis de inferência sem efeitos colaterais. |
| **`api-gateway-cpp`**| C++23, Drogon, redis-plus-plus | Ponto único de entrada REST e WebSocket multiplexado para o frontend, autenticação e rate-limiting. | Baixíssima latência na entrega de pacotes de telemetria serializados e suporte massivo a conexões WS simultâneas. |
| **`apps/web`** | Next.js 15, TypeScript Strict, Tailwind CSS, uPlot, D3, SVG | Dashboard de engenharia de alta fidelidade com visual escuro graphite, crosshair unificado entre telemetria e mapa de pista. | Renderização rápida via Canvas/uPlot, reatividade isolada via Zustand e tipagem ponta a ponta. |

---

## 3. Qualidade, Cobertura e Limitações dos Dados Públicos

Dados de telemetria pública (como OpenF1) possuem características distintas de telemetria proprietária de equipes (ATLAS / Wintax / MoTeC):

1. **Taxa de Amostragem Variável**: OpenF1 agrega dados de broadcast a taxas que oscilam entre 3 Hz e 10 Hz para canais de carro, ao passo que telemetria de ECU oficial opera em 50 Hz – 200 Hz.
2. **Discretização de Marcha e DRS**: O canal de DRS e marchas pode sofrer latência de broadcast (atraso de até 200–500 ms em relação ao sinal real do volante).
3. **Incerteza na Posição Espacial**: Coordenadas X/Y de GPS de broadcast sofrem com oclusões, reflexões multipath e interpolação imprecisa nas zebras.
4. **Política de Transparência do ApexTelemetry**:
   - Todo gráfico e tabela exibe o percentual de cobertura da volta ($n_{\text{amostras}} / n_{\text{esperado}}$).
   - Lacunas temporais $> 500\text{ ms}$ são sinalizadas visualmente como "Incomplete / Discontinuous" e não interpoladas artificialmente.
   - Qualquer inferência do motor de estratégia (Haskell) carrega uma pontuação explícita de confiança (`Confidence: 0.0 - 1.0`) e lista de premissas e limitações.

---

## 4. Pipeline de Processamento Numérico de Voltas

```mermaid
sequenceDiagram
    participant Ingest as Ingest C++
    participant DB as TimescaleDB
    participant Analytics as Analytics C++
    participant Strategy as Strategy Haskell
    participant UI as Web Client

    Ingest->>DB: Ingestão de Raw Telemetry (Speed, Throttle, Brake, RPM, Gear, DRS)
    UI->>Analytics: Requisita comparação (Volta A vs Volta B)
    Analytics->>DB: Busca amostras ordenadas por timestamp
    Analytics->>Analytics: Converte velocidade para m/s
    Analytics->>Analytics: Integração trapezoidal de distância acumulada
    Analytics->>Analytics: Reamostragem em grade espacial fixa (5 metros)
    Analytics->>Analytics: Cálculo de tempo acumulado e Delta de Tempo
    Analytics->>Strategy: Envia segmentos de telemetria e métricas calculadas
    Strategy->>Strategy: Avalia elegibilidade de volta, stints e micro-setores
    Strategy->>Strategy: Dedução lógica de perda/ganho com evidências numéricas
    Strategy-->>Analytics: Retorna lista de Insights auditáveis
    Analytics-->>UI: Retorna ComparisonPayload (Grid 5m, Canais, Deltas, Insights, Qualidade)
    UI->>UI: Renderiza gráficos sincronizados via uPlot e mapa SVG
```

---

## 5. Estratégia de Deploy e Execução Local

O monorepo suporta execução 100% local com orquestração via Docker Compose:
- **TimescaleDB**: Tabela particionada temporalmente por `occurred_at`.
- **Redis**: Armazena sessões ativas e cache de comparações recentes.
- **NATS**: Tópicos `telemetry.raw`, `telemetry.normalized`, `racecontrol.events`.
- **Modo Standalone / Demo**: O frontend conta com um mock determinístico completo baseado em telemetria do GP do Bahrein de 2024 (Verstappen vs Leclerc), explicitamente marcado como fixture de desenvolvimento para permitir trabalho imediato de UI sem dependência de containers externos.
