# ApexTelemetry

> Plataforma de engenharia para análise de telemetria de Fórmula 1 em alta precisão espacial, voltada a analistas técnicos, engenheiros de performance e criadores de conteúdo.

---

## Estrutura do Monorepo

```
apex-telemetry/
├── apps/
│   └── web/                   # Aplicação Next.js 15 (App Router, Tailwind CSS, uPlot/Canvas, SVG)
├── services/
│   ├── api-gateway-cpp/       # Gateway REST e WebSocket Drogon em C++23
│   ├── ingest-cpp/            # Ingestão resiliente OpenF1, normalizador e replay em C++23
│   ├── analytics-cpp/         # Cálculos numéricos e reamostragem espacial em grade de 5m em C++23
│   └── strategy-hs/           # Motor de regras, classificação de voltas e insights explicáveis em Haskell
├── packages/
│   ├── contracts/             # Esquemas versionados (JSON Schema, OpenAPI 3.1, TypeScript Types)
│   └── ui/                    # Design system sóbrio de instrumentação técnica e tokens visuais
├── infra/
│   ├── docker-compose.yml     # PostgreSQL 16 + TimescaleDB, Redis 7, NATS JetStream, Prometheus e Grafana
│   ├── postgres/init.sql      # DDL relacional e hypertables
│   └── observability/         # Configurações do Prometheus e Grafana
└── docs/
    ├── architecture.md        # Arquitetura geral, pipelines e governança de dados
    ├── data-contracts.md      # Contratos e modelos de dados
    ├── quickstart.md          # Guia de execução rápida
    └── adr/                   # Decisões arquiteturais registradas (ADRs 001 a 004)
```

---

## Status da Implementação — Fase 5 Concluída

- [x] Monorepo estruturado com `pnpm`, `CMake` e `Cabal`.
- [x] Design system sóbrio, preciso e escuro de instrumentação de telemetria (`@apex-telemetry/ui`).
- [x] Aplicação web Next.js (`apps/web`) com:
  - Comparação multicanal alinhada por distância (5 metros) com crosshair síncrono.
  - Painéis para Velocidade, Delta de Tempo, Acelerador, Freio, Marcha e DRS.
  - Mapa vetorial interativo do circuito de Sakhir com telemetria espacial sincronizada.
  - Painel de insights explicáveis com evidências empíricas numéricas, premissas, limitações e confiança formal.
  - Painel expansível inferior com Direção de Prova, Stints com degradação de pneus e matriz de voltas.
  - Live Updates via Server-Sent Events (SSE) com hook `useLiveSession` e badge visual no Header.
  - Modo Replay interativo no frontend via `requestAnimationFrame` com velocidade ajustável.
  - ErrorBoundary global e Skeleton loaders durante loading de dados.
- [x] Contratos formais OpenAPI 3.1, JSON Schema e TypeScript (`@apex-telemetry/contracts`).
- [x] Algoritmos matemáticos de integração de distância e reamostragem espacial com 100% de cobertura de testes unitários (`vitest`).
- [x] Infraestrutura Docker Compose com TimescaleDB, Redis, NATS, Prometheus e Grafana auto-provisionado.
- [x] Ingestão histórica OpenF1 em C++23 com persistência raw/normalizada, upsert transacional PostgreSQL e modo `--replay`.
- [x] Motor numérico C++23 para integração de distância, grade espacial de 5 m e delta temporal.
- [x] Motor Haskell de regras de domínio, classificação de voltas, degradação e insights explicáveis com 500 testes QuickCheck.
- [x] Suítes automatizadas completas: Vitest (frontend e stores), CTest (C++23) e QuickCheck (Haskell).
- [x] Roteamento REST e streaming SSE parametrizados, validação semântica e erros JSON previsíveis.
- [x] Endpoint de métricas `/metrics` no formato Prometheus e dashboard pré-provisionado no Grafana.
- [x] Documentação arquitetural completa e registros de decisão (ADR-001 a ADR-005).

Para instruções detalhadas de como rodar e testar o projeto, consulte [docs/quickstart.md](docs/quickstart.md).
