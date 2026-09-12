# Contratos de dados

Fonte de verdade: [`packages/contracts/openapi.yaml`](../packages/contracts/openapi.yaml)
(OpenAPI 3.1). Dele derivam:

- `packages/contracts/schemas/lap-comparison.json` — JSON Schema 2020-12, gerado;
- `packages/contracts/src/index.ts` — tipos TypeScript consumidos pela interface.

Versão do esquema de comparação: **1.2.0**.

---

## Regras que valem para todo payload

**1. A origem é declarada.** Toda resposta carrega `X-Apex-Data-Source`, com
`postgresql` ou `openf1-upstream`. A comparação também traz `data_source` no corpo.

**2. Ausência é `null`, nunca um valor plausível.**

| Campo | `null` significa |
| --- | --- |
| `lap_time_s` | volta não cronometrada (saída de box, bandeira, abandono) — **não** `0` |
| `sector_n_s` | setor não registrado pelo cronômetro |
| `i1/i2/st_speed_kmh` | sensor de passagem sem leitura para aquela volta |
| `coverage_pct` (em `Lap`) | telemetria da volta ainda não medida — **não** `100` |
| `track_temperature_c` | sessão sem leituras meteorológicas |
| `recommendation` | a degradação medida não justifica a parada |

**3. Erros são estruturados e nunca vêm acompanhados de dados.**

```json
{ "error": "Lap 99 of driver 1 has no recorded lap time in this session",
  "code": "LAP_NOT_FOUND" }
```

O `code` é estável e legível por máquina. A distinção de status importa:

| Status | Significado |
| --- | --- |
| `400` / `422` | O pedido está malformado ou fora do domínio válido |
| `404` | A origem respondeu que o recurso não existe |
| `429` | O upstream recusou por excesso de requisições — transitório |
| `502` | Falha ao consultar o upstream — transitório |
| `503` | Uma dependência necessária não está configurada |

Colapsar `429`/`502` em `404` faria o cliente desistir de um dado que existe.

---

## `LapComparison`

O payload central. Além dos canais alinhados, carrega o que permite **julgar** os
canais.

### `quality_audit`

| Campo | Leitura |
| --- | --- |
| `delta_closure_error_s` | \|delta acumulado − diferença cronometrada oficial\|. Verificação independente do alinhamento: o cronômetro da FIA não participa do cálculo. Abaixo de 0,02 s o eixo é confiável; acima, a interface marca em alerta |
| `coverage_pct` | Fração dos nós da grade obtidos por interpolação entre amostras válidas |
| `max_interpolation_gap_m` | Maior distância entre duas amostras brutas consecutivas |
| `median_sample_interval_s` | Cadência real da telemetria (≈0,24 s na OpenF1) |
| `raw_samples_ref/comp` | Quantas amostras de ECU sustentam cada volta |
| `discontinuous_segments` | Lacunas acima do limite adaptativo |

### `corners`

Zonas lentas detectadas no canal de velocidade. **`C1..Cn` é a ordem de aparição
na volta, não a numeração oficial do circuito.** A telemetria a ~4 Hz não resolve
curvas tomadas em carga plena; em Sakhir são detectadas 6 a 7 zonas para 15 curvas
oficiais. Rotulá-las como "T7" afirmaria uma correspondência que o dado não
sustenta.

### `speed_traps`

O campo `origin` separa duas naturezas:

- `openf1_marshalling_loop` — velocidade do sensor oficial (I1, I2, speed trap). A
  posição no eixo é obtida percorrendo o canal de tempo até o instante do setor.
- `channel_peak` — pico de velocidade medido nos canais, com proeminência mínima.

### `insights`

`engine` diz quem escreveu: `strategy-hs` (motor Haskell) ou `analytics-cpp`
(resumo determinístico local). `evidence.cause` traz a causa dominante escolhida
pelo motor numérico; `assumptions` e `limitations` são derivadas dos fatos da
comparação — compostos diferentes, diferença de idade de pneu, confiança baixa ou
perda próxima da resolução da grade aparecem como itens declarados.

---

## `CircuitGeometry`

Traçado reconstruído do transponder de posição da volta mais rápida da sessão,
com a distância de cada ponto obtida pela integração da telemetria da mesma volta.

```json
{ "path": [[12.2, -371.0, 1472.0], [25.8, -369.0, 1498.0]] }
```

Cada ponto é `[distância_m, x, y]` nas unidades do transponder da OpenF1 (décimos
de metro). O cliente ajusta a escala pelo bounding box; não há sistema de
coordenadas absoluto a respeitar.

---

## `StintAnalysis`

`observed_degradation_s_per_lap` e `predicted_pace_loss_s` são publicados lado a
lado de propósito: o primeiro é a inclinação medida na regressão dos tempos reais,
o segundo é o modelo térmico paramétrico. Colapsar os dois em um número esconderia
justamente a informação interessante — o quanto o stint real se afastou do modelo.

`representative_laps` diz quantas voltas sobreviveram ao filtro de outliers de
tráfego. Abaixo de 3, a inclinação não é estatisticamente utilizável e `notes`
declara isso.

---

## Esquema relacional

Definido em [`infra/postgres/init.sql`](../infra/postgres/init.sql). Duas
hypertables TimescaleDB (`telemetry_samples`, `location_samples`) e a tabela de
auditoria `raw_payloads`, que guarda o corpo original de cada resposta da OpenF1
indexado pelo digest SHA-256 — é o que permite reprocessar uma análise e provar de
qual payload cada número veio.
