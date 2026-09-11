# ApexTelemetry — Contratos de Dados e Esquemas de Mensageria

Versão: `1.0.0`  
Status: Ativo

---

## 1. Princípios de Modelagem

1. **Imutabilidade de Dados Brutos**: Toda informação ingerida de fontes externas (OpenF1, Ergast, Live Timing) é gravada sem perdas com identificador de ingestão, hash e timestamp UTC de recebimento.
2. **Tempo sem Ambiguidade**: Proibido uso de ponto flutuante para horários de parede. Em wire protocols (JSON/REST), horários são strings ISO 8601 em UTC (`YYYY-MM-DDTHH:MM:SS.sssZ`). Internamente em C++, `std::chrono::microseconds` desde a época Unix. Em Haskell, `Data.Time.Clock.UTCTime`.
3. **Identificação e Versionamento**: Toda mensagem carrega `schema_version`, `source` (`"openf1"` ou `"f1_live"`), `session_key` e flags de auditoria de qualidade.

---

## 2. Entidades Principais

### 2.1. TelemetrySample (Amostra Bruta / Normalizada)
```json
{
  "schema_version": "1.0.0",
  "session_key": 9472,
  "driver_number": 1,
  "occurred_at": "2024-03-01T16:24:12.350Z",
  "speed_kmh": 298.4,
  "throttle_pct": 100.0,
  "brake_pct": 0.0,
  "rpm": 11450,
  "gear": 7,
  "drs_state": 12,
  "quality_flags": {
    "is_interpolated": false,
    "gps_lock": true,
    "latency_ms": 142
  }
}
```

### 2.2. DistanceAlignedSample (Ponto na Grade de 5 Metros)
```json
{
  "distance_m": 1250.0,
  "elapsed_time_s": 18.420,
  "speed_kmh": 245.2,
  "throttle_pct": 82.0,
  "brake_pct": 0.0,
  "rpm": 10850,
  "gear": 5,
  "drs_active": false
}
```

### 2.3. LapComparison (Resultado da Comparação Espacial)
```json
{
  "schema_version": "1.0.0",
  "session_key": 9472,
  "circuit_key": 63,
  "circuit_name": "Bahrain International Circuit",
  "grid_step_m": 5.0,
  "total_distance_m": 5412.0,
  "reference_lap": {
    "driver_number": 1,
    "driver_code": "VER",
    "lap_number": 14,
    "lap_time_s": 89.179,
    "compound": "SOFT",
    "stint_lap": 3,
    "coverage_pct": 99.4,
    "samples_count": 1083
  },
  "comparison_lap": {
    "driver_number": 16,
    "driver_code": "LEC",
    "lap_number": 15,
    "lap_time_s": 89.407,
    "compound": "SOFT",
    "stint_lap": 3,
    "coverage_pct": 98.9,
    "samples_count": 1083
  },
  "channels": [
    {
      "distance_m": 1250.0,
      "ref": {
        "time_s": 18.210,
        "speed_kmh": 252.0,
        "throttle_pct": 90.0,
        "brake_pct": 0.0,
        "rpm": 11020,
        "gear": 6,
        "drs": false
      },
      "comp": {
        "time_s": 18.450,
        "speed_kmh": 245.2,
        "throttle_pct": 82.0,
        "brake_pct": 0.0,
        "rpm": 10850,
        "gear": 5,
        "drs": false
      },
      "delta_time_s": 0.240
    }
  ],
  "insights": [
    {
      "id": "ins-t4-exit",
      "driver_code": "LEC",
      "distance_start_m": 1420.0,
      "distance_end_m": 2080.0,
      "time_loss_s": 0.240,
      "summary": "Piloto 16 perdeu 0,24 s entre 1,42 km e 2,08 km. A velocidade mínima foi 6,8 km/h menor e a aceleração acima de 95% ocorreu 31 m mais tarde.",
      "evidence": {
        "min_speed_ref_kmh": 118.2,
        "min_speed_comp_kmh": 111.4,
        "full_throttle_distance_ref_m": 1640.0,
        "full_throttle_distance_comp_m": 1671.0,
        "braking_point_diff_m": -4.2
      },
      "assumptions": [
        "Nenhum tráfego detectado na janela de 100 m à frente",
        "Ambos os pilotos em modo de classificação ICE/ERS"
      ],
      "limitations": [
        "Dados OpenF1 discretizados a 10 Hz; incerteza espacial de frenagem estimada em ±2,8 m"
      ],
      "confidence": 0.94
    }
  ],
  "quality_audit": {
    "max_interpolation_gap_m": 12.4,
    "discontinuous_segments": 0,
    "confidence_score": 0.98
  }
}
```

### 2.4. RaceControlEvent
```json
{
  "schema_version": "1.0.0",
  "session_key": 9472,
  "occurred_at": "2024-03-01T16:32:05.100Z",
  "category": "Flag",
  "flag": "YELLOW",
  "sector": 2,
  "message": "YELLOW FLAG IN SECTOR 2",
  "driver_number": null,
  "lap_number": 8
}
```
