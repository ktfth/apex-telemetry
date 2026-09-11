# ApexTelemetry — Guia de Inicialização Rápida (Quickstart)

Este guia orienta a inicialização e teste local do ecossistema ApexTelemetry.

---

## 1. Pré-requisitos
- Node.js >= 20 (recomendado 26+) e `pnpm` >= 12
- Compilador C++23 (`g++ >= 14` ou `clang++ >= 18`) e `cmake >= 3.25`
- GHC (8.10+ ou 9.x) e `cabal`
- Docker e Docker Compose (para PostgreSQL + TimescaleDB, Redis e NATS)

---

## 2. Inicialização da Infraestrutura Local

Inicie os serviços de persistência e mensageria em segundo plano:
```bash
docker compose -f infra/docker-compose.yml up -d
```
Verifique a saúde dos serviços:
- **TimescaleDB**: `localhost:5432` (`apex_telemetry`)
- **Redis**: `localhost:6379`
- **NATS JetStream**: `localhost:4222` (Monitor em `localhost:8222`)
- **Prometheus**: `localhost:9090`
- **Grafana**: `localhost:3001` (login `admin` / `apex_admin`)

---

## 3. Frontend de Telemetria (Next.js)

Instale as dependências e inicie o ambiente de desenvolvimento:
```bash
pnpm install
pnpm dev
```
Acesse a aplicação em [http://localhost:3000](http://localhost:3000).

### Comandos de Validação de Código:
- **Testes Unitários (Vitest)**:
  ```bash
  pnpm test
  ```
- **Typecheck Estrito (TypeScript)**:
  ```bash
  pnpm typecheck
  ```
- **Build de Produção**:
  ```bash
  pnpm --filter @apex-telemetry/web build
  ```

---

## 4. Compilação dos Serviços Nativos

### 4.1. Serviços C++23
```bash
cmake -B build -S .
cmake --build build
```

### 4.2. Motor de Domínio e Regras (Haskell)
```bash
cd services/strategy-hs
cabal build
cabal run strategy-hs
```
