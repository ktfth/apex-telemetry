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

## Status da Implementação — Fase 1 Concluída

- [x] Monorepo estruturado com `pnpm`, `CMake` e `Cabal`.
- [x] Design system sóbrio, preciso e escuro de instrumentação de telemetria (`@apex-telemetry/ui`).
- [x] Aplicação web Next.js (`apps/web`) com:
  - Comparação multicanal alinhada por distância (5 metros) com crosshair síncrono.
  - Painéis para Velocidade, Delta de Tempo, Acelerador, Freio, Marcha e DRS.
  - Mapa vetorial interativo do circuito de Sakhir com telemetria espacial sincronizada.
  - Painel de insights explicáveis com evidências empíricas numéricas, premissas, limitações e confiança formal.
  - Painel expansível inferior com Direção de Prova, Stints com degradação de pneus e matriz de voltas.
  - Dados demo do GP do Bahrein 2024 explicitamente declarados e auditados como sintéticos.
- [x] Contratos formais OpenAPI 3.1, JSON Schema e TypeScript (`@apex-telemetry/contracts`).
- [x] Algoritmos matemáticos de integração de distância e reamostragem espacial com 100% de cobertura de testes unitários (`vitest`).
- [x] Infraestrutura Docker Compose com TimescaleDB, Redis, NATS, Prometheus e Grafana.
- [x] Esqueletos de serviços C++23 e Haskell tipados e compiláveis.
- [x] Documentação arquitetural completa e registros de decisão (ADR-001 a ADR-004).

Para instruções detalhadas de como rodar e testar o projeto, consulte [docs/quickstart.md](docs/quickstart.md).
