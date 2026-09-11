# ADR-003: Modelagem de Regras de Domínio e Explicabilidade em Haskell

## Status
Aceito

## Contexto
Sistemas de recomendação de engenharia e análise de estratégia em motorsport frequentemente sofrem de regras opacas, heurísticas mágicas ou generalizações vazias ("o piloto foi mais lento"). Para telemetria de nível de engenharia, cada afirmação precisa ser:
1. **Determinística**: Dada a mesma telemetria e eventos de pista, a conclusão é estritamente idêntica.
2. **Baseada em Evidências Concretas**: Conter valores numéricos observados (ex: velocidade mínima, ponto de frenagem, início de aceleração acima de 95%).
3. **Imune a Estados Inválidos**: Uma volta sob Safety Car ou volta de entrada aos boxes (`in-lap`) não pode ser classificada como representativa de ritmo de corrida.

## Decisão
Isolamos a lógica de estratégia, classificação e geração de insights no serviço **`strategy-hs`** em Haskell puro:

- **Tipos Algébricos Rígidos**:
  ```haskell
  data LapKind = FlyingLap | OutLap | InLap | InvalidLap LapInvalidReason | SafetyCarAffected
  data TyreCompound = Soft | Medium | Hard | Intermediate | Wet
  data Evidence = Evidence
    { distanceStartM :: !Double
    , distanceEndM   :: !Double
    , timeDeltaS     :: !Double
    , speedDiffKmh   :: !Double
    , throttleDelayM :: !Double
    , brakePointDiffM:: !Double
    }
  data Insight = Insight
    { insightId   :: !Text
    , driverNum   :: !Int
    , kind        :: !InsightKind
    , summaryText :: !Text
    , evidence    :: !Evidence
    , assumptions :: ![Text]
    , limitations :: ![Text]
    , confidence  :: !Double
    }
  ```
- **Testes Baseados em Propriedades**: Uso de `tasty-quickcheck` para garantir que invariantes (ex: tempo total é monotônico, stints não se sobrepõem, voltas inválidas geram confidence < 0.3) nunca sejam violadas.

## Consequências
- Separação nítida: C++ calcula dados numéricos em alta frequência; Haskell valida o significado de domínio e gera o texto explicável.
- Eliminação de falsos positivos na identificação de stints e degradação de pneus.
