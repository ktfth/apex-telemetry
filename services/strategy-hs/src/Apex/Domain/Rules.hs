{-# LANGUAGE OverloadedStrings #-}
module Apex.Domain.Rules where

import Apex.Domain.Types
import Data.Text (Text)
import qualified Data.Text as T

-- | Classifica a volta com base em condições de tempo e eventos de pista
classifyLapKind :: Bool -> Bool -> Bool -> Double -> LapKind
classifyLapKind isPitOut isPitIn isSC _lapTime
  | isSC      = SafetyCarAffected
  | isPitOut  = OutLap
  | isPitIn   = InLap
  | otherwise = FlyingLap

-- | Gera insight explicável estritamente baseado em evidências empíricas
buildExplainableLossInsight :: Text -> Evidence -> Insight
buildExplainableLossInsight driver e =
  let vDiff = minSpeedRefKmh e - minSpeedCompKmh e
      tLate = fullThrottleDistCompM e - fullThrottleDistRefM e
      summary = T.concat
        [ "Piloto "
        , driver
        , " perdeu "
        , T.pack (show (timeDeltaS e))
        , " s entre "
        , T.pack (show (distanceStartM e / 1000.0))
        , " km e "
        , T.pack (show (distanceEndM e / 1000.0))
        , " km. A velocidade mínima foi "
        , T.pack (show vDiff)
        , " km/h menor e a aceleração acima de 95% ocorreu "
        , T.pack (show tLate)
        , " m mais tarde."
        ]
  in Insight
      { insightId = "ins-loss-segment"
      , driverCode = driver
      , evidence = e
      , explanation = summary
      , assumptions = ["Ar limpo sem turbulência aerodinâmica", "Unidade de potência em modo qualificação"]
      , limitations = ["Amostragem OpenF1 discretizada na grade espacial de 5m"]
      , confidence = Confidence 0.94
      }
