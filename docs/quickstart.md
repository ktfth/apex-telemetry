# Guia de execução

Quatro processos compõem a plataforma. O mínimo para ter a interface funcionando
com dados reais são dois: o **gateway** e a **web**. Os outros dois acrescentam
capacidade, não sustentação.

| Processo | Papel | Obrigatório? |
| --- | --- | --- |
| `apex_api_gateway` | API REST + SSE, motor de alinhamento espacial | **sim** |
| `apps/web` | Interface de análise | **sim** |
| `strategy-hs` | Explicações e degradação de pneus | opcional — sem ele, as explicações caem no resumo determinístico em C++ e `/analysis/degradation` responde 503 |
| `apex_ingest` + PostgreSQL | Armazém local | opcional — sem ele, o gateway consulta a OpenF1 ao vivo |

---

## 1. Dependências

```bash
# Arch Linux
sudo pacman -S --needed cmake gcc curl postgresql-libs nodejs pnpm ghc cabal-install

# Debian/Ubuntu
sudo apt install -y cmake g++ libcurl4-openssl-dev libpq-dev nodejs ghc cabal-install
```

Requisitos: compilador com C++23 (GCC 14+ ou Clang 18+), CMake 3.25+, Node 20+,
GHC 8.10+.

---

## 2. Compilar

```bash
# C++23 — gateway, ingestão e motor de análise
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"

# Haskell — motor de domínio
(cd services/strategy-hs && cabal build all)

# TypeScript — contratos, design system e aplicação
pnpm install
```

---

## 3. Subir

```bash
# 1. Motor de domínio (opcional, porta 8092)
(cd services/strategy-hs && APEX_STRATEGY_PORT=8092 "$(cabal list-bin strategy-hs)") &

# 2. Gateway (porta 8080)
APEX_STRATEGY_URL=http://127.0.0.1:8092 ./build/services/api-gateway-cpp/apex_api_gateway 8080 &

# 3. Interface (porta 3000)
pnpm dev
```

Abra <http://localhost:3000>. Escolha um ano, uma sessão, dois pilotos e duas
voltas cronometradas.

Confirme que a pilha está sã:

```bash
node scripts/verify-stack.mjs
```

O script exercita cada endpoint contra dados reais e confere as invariantes que
tornam a análise confiável — entre elas o fechamento do delta acumulado contra o
cronômetro oficial da FIA.

---

## 4. Armazém local (opcional)

Sem `APEX_DATABASE_URL`, o gateway consulta a OpenF1 a cada requisição e mantém um
cache em memória com TTL de 5 minutos. Isso funciona, mas a OpenF1 impõe limite de
requisições e a latência de uma comparação fria fica em alguns segundos. Ingerir a
sessão elimina as duas coisas.

```bash
cd infra && docker compose up -d timescaledb
export APEX_DATABASE_URL="postgresql://apex_user:apex_secure_pass@localhost:5432/apex_telemetry"

# Descobrir sessões
./build/services/ingest-cpp/apex_ingest --year 2024 --country Bahrain --list

# Ingerir catálogo + telemetria das 3 voltas mais rápidas de cada piloto
./build/services/ingest-cpp/apex_ingest --session 9468 --telemetry --location --fastest 3
```

Reinicie o gateway com `APEX_DATABASE_URL` no ambiente. O cabeçalho
`X-Apex-Data-Source` passa a responder `postgresql`, e o badge da interface muda de
`OPENF1 AO VIVO` para `POSTGRES`.

A ingestão é idempotente: reexecutar converge para o mesmo estado. Cada resposta da
OpenF1 é arquivada byte a byte em `data/raw/<endpoint>/`, nomeada pelo digest
SHA-256 do conteúdo, e registrada em `raw_payloads` — é o que permite reprocessar
uma análise meses depois e provar de qual payload cada número veio.

### Opções da ingestão

```
--session <chave>      Sessão OpenF1 a ingerir
--year / --country / --session-name / --list    Descoberta de sessões
--drivers 1,16,44      Restringe os pilotos (padrão: todos)
--laps 14,15           Voltas exatas para telemetria de alta frequência
--fastest <n>          As n voltas mais rápidas de cada piloto (padrão: 3)
--telemetry            Baixa as amostras de ECU (car_data)
--location             Baixa as amostras de posição (traçado real)
--dry-run              Busca e relata, sem escrever
```

---

## 5. Testes

```bash
cmake --build build -j"$(nproc)" && (cd build && ctest --output-on-failure)
(cd services/strategy-hs && cabal test)
pnpm test
pnpm typecheck
node scripts/verify-stack.mjs   # exige o gateway no ar
```

---

## 6. Observabilidade

```bash
cd infra && docker compose up -d prometheus grafana
```

- Prometheus: <http://localhost:9090> — raspa `/metrics` do gateway.
- Grafana: <http://localhost:3001> (admin / `apex_admin`).

As métricas incluem latência por rota, origem efetiva de cada resposta
(`apex_requests_by_source_total`), tráfego e recusas do upstream e falhas do motor
Haskell.

---

## 7. Variáveis de ambiente

| Variável | Serviço | Padrão | Efeito |
| --- | --- | --- | --- |
| `APEX_DATABASE_URL` | gateway, ingestão | — | Sem ela, o gateway usa a OpenF1 ao vivo e a ingestão só arquiva em disco |
| `APEX_STRATEGY_URL` | gateway | — | Sem ela, as explicações vêm do resumo em C++ e a degradação responde 503 |
| `APEX_STRATEGY_PORT` | strategy-hs | `8092` | Porta do motor de domínio |
| `APEX_HTTP_IP_FAMILY` | gateway, ingestão | `v4` | `auto`/`v4`/`v6`. O padrão é IPv4 porque em redes dual-stack com AAAA inalcançável a thread de resolução do libcurl trava em `poll` e `curl_easy_cleanup` bloqueia |
| `APEX_HTTP_TIMEOUT_S` | gateway, ingestão | `25` | Timeout por requisição ao upstream |
| `NEXT_PUBLIC_API_URL` | web | `http://localhost:8080/api/v1` | Endereço do gateway |

---

## Solução de problemas

**A interface diz `GATEWAY OFFLINE`.** O gateway não está no ar ou está em outra
porta. Verifique `curl localhost:8080/api/v1/health` e `NEXT_PUBLIC_API_URL`.

**`UPSTREAM_RATE_LIMITED` (HTTP 429).** A OpenF1 recusou por excesso de
requisições. O cache do gateway absorve a maior parte; para eliminar de vez,
ingira a sessão para PostgreSQL.

**`TELEMETRY_UNAVAILABLE` em uma volta específica.** A OpenF1 não tem amostras de
ECU suficientes para aquela volta — comum em voltas de saída de box e em sessões
interrompidas. Escolha outra volta; o painel marca a mais rápida de cada piloto.

**A degradação responde 503.** `APEX_STRATEGY_URL` não está configurada ou o motor
Haskell não está no ar.

**Nenhuma sessão aparece para o ano escolhido.** A cobertura de telemetria de carro
da OpenF1 começa em 2023.
