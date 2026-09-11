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

## 3. Ingestão e API Gateway C++23

### 3.1. Compilação Completa via CMake
```bash
cmake -B build -S .
cmake --build build
```
Executáveis gerados:
- `build/services/api-gateway-cpp/apex_api_gateway`
- `build/services/ingest-cpp/apex_ingest`
- `build/services/analytics-cpp/apex_analytics`

### 3.2. Gerar Dados Normalizados e Ingestão
```bash
# Modo offline (gera dados normalizados a partir do audit fixture)
./build/services/ingest-cpp/apex_ingest --offline

# Modo online (ingestão direta da OpenF1 com retry e rate limiting)
./build/services/ingest-cpp/apex_ingest --session 9472 --year 2024
```

### 3.3. Iniciar o API Gateway REST
```bash
./build/services/api-gateway-cpp/apex_api_gateway 8080
```
Endpoints disponíveis:
- `GET http://localhost:8080/api/v1/health`
- `GET http://localhost:8080/api/v1/sessions`
- `GET http://localhost:8080/api/v1/sessions/9472/drivers`
- `GET http://localhost:8080/api/v1/sessions/9472/laps`
- `GET http://localhost:8080/api/v1/sessions/9472/race-control`

---

## 4. Frontend de Telemetria (Next.js)

Instale as dependências e inicie o ambiente de desenvolvimento:
```bash
pnpm install
pnpm dev
```
Acesse a aplicação em [http://localhost:3000](http://localhost:3000).

O frontend detectará automaticamente se o `apex_api_gateway` está online na porta 8080 e exibirá o indicador verde de baixa latência (`API GATEWAY C++23 [ONLINE] | <latency>ms`), ou fará fallback suave e seguro para a fixture de teste devidamente rotulada (`STANDALONE DEMO`).

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

## 5. Motor de Domínio e Regras (Haskell)
```bash
cd services/strategy-hs
cabal build
cabal run strategy-hs
```
