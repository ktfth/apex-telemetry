# Respostas gravadas

Payloads **reais** do `api-gateway-cpp`, capturados de Bahrain 2024 Qualifying
(sessão 9468, VER volta 16 contra LEC volta 15), usados nos testes de renderização.

Não são fixtures inventadas: cada número aqui saiu da telemetria da OpenF1 através
do motor de alinhamento espacial. Os canais e a polilinha do traçado foram
subamostrados (1 em 20 e 1 em 4) para manter os arquivos legíveis; todo o resto
está exatamente como o gateway devolveu.

Regravar após uma mudança no contrato:

```bash
curl -s "http://localhost:8080/api/v1/analysis/compare?session_key=9468\
&ref_driver=1&ref_lap=16&comp_driver=16&comp_lap=15" > /tmp/cmp.json
curl -s "http://localhost:8080/api/v1/sessions/9468/circuit" > /tmp/circ.json
```
