{-# LANGUAGE OverloadedStrings #-}
{-# LANGUAGE StrictData #-}

-- | Regras de domínio e geração de explicações auditáveis.
--
-- Toda frase produzida aqui cita um número que veio da telemetria medida. Não há
-- texto decorativo nem valor estimado sem origem no dado de entrada.
module Apex.Domain.Rules
  ( classifyLap
  , isLapEligibleForPaceAnalysis
  , validateStintNonOverlap
  , formatFixed
  , generateInsight
  , generateInsights
  , insightAssumptions
  , insightLimitations
  ) where

import Apex.Domain.Types
import Data.Text (Text)
import qualified Data.Text as T
import Text.Printf (printf)

-- | Classifica a volta a partir dos fatos reportados pelo cronômetro oficial.
classifyLap :: Bool -> Bool -> Bool -> Maybe Text -> LapKind
classifyLap isPitOut isPitIn isSC trackLimitsReason
  | isSC = SafetyCarAffected
  | isPitOut = OutLap
  | isPitIn = InLap
  | Just reason <- trackLimitsReason = InvalidLap reason
  | otherwise = FlyingLap

-- | Apenas voltas cronometradas limpas e com cobertura alta entram no ritmo.
isLapEligibleForPaceAnalysis :: Lap -> Bool
isLapEligibleForPaceAnalysis lap =
  case lapKind lap of
    FlyingLap -> coveragePct lap >= 98.0
    _ -> False

-- | Stints sucessivos de um mesmo piloto não podem se sobrepor.
validateStintNonOverlap :: StintObservation -> StintObservation -> Bool
validateStintNonOverlap s1 s2
  | obsDriverNumber s1 /= obsDriverNumber s2 = True
  | obsStintNumber s1 == obsStintNumber s2 = False
  | obsStintNumber s1 < obsStintNumber s2 = lastLap s1 < firstLap s2
  | otherwise = lastLap s2 < firstLap s1
  where
    firstLap s = minimum (maxBound : map stintLapNumber (obsLaps s))
    lastLap s = maximum (minBound : map stintLapNumber (obsLaps s))

-- | Formatação numérica estável, independente de locale.
formatFixed :: Int -> Double -> Text
formatFixed places value = T.pack (printf ("%." ++ show places ++ "f") value)

-- | Regime da curva a partir da velocidade real de ápice.
cornerRegime :: Double -> Text
cornerRegime apexSpeed
  | apexSpeed < 110.0 = "curva lenta, limitada por tração"
  | apexSpeed < 190.0 = "curva de média velocidade, limitada por aderência mecânica"
  | otherwise = "curva rápida, limitada por carga aerodinâmica"

-- | Descreve a velocidade mínima do trecho sem afirmar equivalência que os
-- números não sustentam: até 2 km/h de diferença é ruído de medição a 4 Hz,
-- acima disso o fato é declarado como diferença.
minimumSpeedClause :: Segment -> Text
minimumSpeedClause segment
  | abs deficit <= 2.0 =
      T.concat
        [ "A velocidade mínima do trecho foi equivalente ("
        , formatFixed 1 (minSpeedCompKmh segment)
        , " contra "
        , formatFixed 1 (minSpeedRefKmh segment)
        , " km/h)."
        ]
  | otherwise =
      T.concat
        [ "A velocidade mínima também foi "
        , formatFixed 1 (abs deficit)
        , if deficit > 0 then " km/h menor (" else " km/h maior ("
        , formatFixed 1 (minSpeedCompKmh segment)
        , " contra "
        , formatFixed 1 (minSpeedRefKmh segment)
        , " km/h), mas a retomada domina a perda."
        ]
  where
    deficit = minSpeedRefKmh segment - minSpeedCompKmh segment

-- | Frase principal: sempre a causa dominante, com os números que a sustentam.
causeNarrative :: Text -> Segment -> Text
causeNarrative driver segment =
  case segmentCause segment of
    ApexSpeed ->
      T.concat
        [ driver
        , if segmentIsCorner segment then " passou pelo ápice a " else " passou pelo ponto mais lento do trecho a "
        , formatFixed 1 (minSpeedCompKmh segment)
        , " km/h contra "
        , formatFixed 1 (minSpeedRefKmh segment)
        , " km/h da referência ("
        , formatFixed 1 (minSpeedRefKmh segment - minSpeedCompKmh segment)
        , " km/h a menos) em "
        , cornerRegime (minSpeedRefKmh segment)
        , "."
        ]
    ThrottleApplication ->
      T.concat
        [ driver
        , " só atingiu 95% de acelerador em "
        , formatFixed 0 (fullThrottleDistCompM segment)
        , " m, "
        , formatFixed 0 (fullThrottleDistCompM segment - fullThrottleDistRefM segment)
        , " m depois da referência"
        , if segmentIsCorner segment then " na saída da curva" else " neste trecho"
        , ". "
        , minimumSpeedClause segment
        ]
    BrakingPoint ->
      T.concat
        [ driver
        , " iniciou a freada em "
        , formatFixed 0 (brakingPointCompM segment)
        , " m, "
        , formatFixed 0 (abs (brakingPointDiffM segment))
        , " m "
        , if brakingPointDiffM segment < 0 then "antes" else "depois"
        , " da referência, que freou em "
        , formatFixed 0 (brakingPointRefM segment)
        , " m."
        ]
    TopSpeed ->
      T.concat
        [ driver
        , " atingiu "
        , formatFixed 1 (maxSpeedCompKmh segment)
        , " km/h de ponta contra "
        , formatFixed 1 (maxSpeedRefKmh segment)
        , " km/h da referência ("
        , formatFixed 1 (maxSpeedRefKmh segment - maxSpeedCompKmh segment)
        , " km/h a menos) neste trecho de reta."
        ]
    MixedCause ->
      T.concat
        [ "Nenhum canal isolado explica a perda de "
        , driver
        , " neste trecho: velocidade mínima "
        , formatFixed 1 (minSpeedRefKmh segment - minSpeedCompKmh segment)
        , " km/h abaixo e retomada "
        , formatFixed 0 (fullThrottleDistCompM segment - fullThrottleDistRefM segment)
        , " m mais tarde, ambos dentro da tolerância de medição."
        ]

-- | Premissas declaradas: derivadas dos fatos da sessão, não de texto fixo.
insightAssumptions :: InsightRequest -> [Text]
insightAssumptions request =
  concat
    [ [ T.concat
          [ "Ambas as voltas foram cronometradas em "
          , sessionName (requestSession request)
          , " no mesmo traçado ("
          , circuitName (requestSession request)
          , ")."
          ]
      ]
    , [ T.concat
          [ "Temperatura de pista reportada: "
          , formatFixed 1 temperature
          , " °C."
          ]
      | Just temperature <- [trackTemperatureC (requestSession request)]
      ]
    , [ "Compostos idênticos nas duas voltas ("
          <> renderCompound (refCompound (requestReference request))
          <> ")."
      | refCompound (requestReference request) == refCompound (requestComparison request)
      , refCompound (requestReference request) /= UnknownCompound
      ]
    ]

-- | Limitações declaradas: cada uma corresponde a uma diferença real observada.
insightLimitations :: InsightRequest -> Segment -> [Text]
insightLimitations request segment =
  concat
    [ [ T.concat
          [ "Compostos diferentes: referência em "
          , renderCompound (refCompound (requestReference request))
          , " e comparação em "
          , renderCompound (refCompound (requestComparison request))
          , "; parte do delta é aderência de pneu, não pilotagem."
          ]
      | refCompound (requestReference request) /= refCompound (requestComparison request)
      ]
    , [ T.concat
          [ "Diferença de "
          , T.pack (show ageGap)
          , " voltas na idade do pneu ("
          , T.pack (show (refTyreAgeLaps (requestReference request)))
          , " contra "
          , T.pack (show (refTyreAgeLaps (requestComparison request)))
          , "); a degradação contribui para o delta."
          ]
      | let ageGap = abs (refTyreAgeLaps (requestReference request) - refTyreAgeLaps (requestComparison request))
      , ageGap >= 3
      ]
    , [ "Confiança do trecho em "
          <> formatFixed 2 (segmentConfidence segment)
          <> ": há lacunas na telemetria dentro desta janela."
      | segmentConfidence segment < 0.85
      ]
    , [ "Perda de "
          <> formatFixed 3 (timeLossS segment)
          <> " s está próxima da resolução da grade espacial; trate como indicativa."
      | timeLossS segment < 0.05
      ]
    ]

-- | Gera um insight explicável a partir de um trecho medido.
generateInsight :: InsightRequest -> Segment -> Insight
generateInsight request segment =
  Insight
    { insightId = segmentId segment
    , driverCode = driver
    , insightSegment = segment
    , explanation =
        T.concat
          [ cornerLabel segment
          , " ("
          , formatFixed 0 (distanceStartM segment)
          , "–"
          , formatFixed 0 (distanceEndM segment)
          , " m): "
          , formatFixed 3 (timeLossS segment)
          , " s perdidos. "
          , causeNarrative driver segment
          ]
    , assumptions = insightAssumptions request
    , limitations = insightLimitations request segment
    , confidence = mkConfidence (segmentConfidence segment * evidenceStrength)
    }
  where
    driver = refDriverCode (requestComparison request)
    -- A força da evidência cresce com a magnitude da perda: um delta de 0,3 s é
    -- inequívoco, um de 0,02 s está no limite da resolução da medição.
    evidenceStrength =
      let loss = abs (timeLossS segment)
       in min 1.0 (0.70 + loss * 2.0)

generateInsights :: InsightRequest -> [Insight]
generateInsights request = map (generateInsight request) (requestSegments request)
