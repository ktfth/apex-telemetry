{-# LANGUAGE OverloadedStrings #-}
{-# LANGUAGE StrictData #-}

-- | Modelo de degradação de pneus.
--
-- A degradação *observada* sai de uma regressão linear sobre os tempos de volta
-- reais do stint. A degradação *prevista* sai de um modelo térmico paramétrico
-- alimentado pela temperatura de pista efetivamente reportada na sessão. As duas
-- são publicadas lado a lado justamente para que a diferença entre medição e
-- modelo fique visível em vez de escondida atrás de um único número.
module Apex.Domain.Degradation
  ( linearDegradation
  , representativeLaps
  , tyreCliffLap
  , isTyreInCliff
  , predictTyrePaceLoss
  , analyseStint
  , analyseStints
  , recommendNextPitStop
  , pitLaneLossS
  ) where

import Apex.Domain.Rules (formatFixed)
import Apex.Domain.Types
import Data.List (sort)
import Data.Maybe (fromMaybe)
import qualified Data.Text as T

-- | Custo típico de uma passagem pelos boxes, em segundos de volta.
pitLaneLossS :: Double
pitLaneLossS = 22.0

-- | Regressão linear simples: inclinação de tempo de volta por volta de uso.
linearDegradation :: [(Double, Double)] -> Double
linearDegradation points
  | length points < 3 = 0.0
  | denominator == 0.0 = 0.0
  | otherwise = numerator / denominator
  where
    n = fromIntegral (length points)
    meanX = sum (map fst points) / n
    meanY = sum (map snd points) / n
    numerator = sum [(x - meanX) * (y - meanY) | (x, y) <- points]
    denominator = sum [(x - meanX) ** 2 | (x, _) <- points]

-- | Descarta voltas não representativas: inválidas e outliers de tráfego ou
-- resfriamento, definidos como acima da mediana mais 3% do tempo mediano.
representativeLaps :: [StintLap] -> [StintLap]
representativeLaps laps
  | length valid < 3 = valid
  | otherwise = filter (\l -> stintLapTimeS l <= threshold) valid
  where
    valid = filter (\l -> stintLapValid l && stintLapTimeS l > 0.0) laps
    times = sort (map stintLapTimeS valid)
    medianTime = times !! (length times `div` 2)
    threshold = medianTime * 1.03

-- | Volta em que o composto perde aderência de forma acentuada.
tyreCliffLap :: TyreCompound -> Int
tyreCliffLap Soft = 16
tyreCliffLap Medium = 26
tyreCliffLap Hard = 38
tyreCliffLap Intermediate = 22
tyreCliffLap Wet = 28
tyreCliffLap UnknownCompound = 20

isTyreInCliff :: TyreCompound -> Int -> Bool
isTyreInCliff compound lapsUsed = lapsUsed >= tyreCliffLap compound

-- | Perda de ritmo acumulada prevista, em segundos, para um composto com
-- @lapsUsed@ voltas de uso a @trackTempC@ graus de pista.
predictTyrePaceLoss :: TyreCompound -> Int -> Double -> Double
predictTyrePaceLoss compound lapsUsed trackTempC =
  (baseRate * lapsF * tempFactor) + cliffPenalty
  where
    baseRate = case compound of
      Soft -> 0.085
      Medium -> 0.052
      Hard -> 0.031
      Intermediate -> 0.110
      Wet -> 0.140
      UnknownCompound -> 0.060
    -- Acima de 35 °C a pista acelera a degradação de forma aproximadamente linear.
    tempFactor = 1.0 + max 0.0 (trackTempC - 35.0) * 0.015
    lapsF = fromIntegral (max 0 lapsUsed)
    cliff = tyreCliffLap compound
    cliffPenalty =
      if lapsUsed > cliff
        then 0.25 * (fromIntegral (lapsUsed - cliff) ** 1.5)
        else 0.0

-- | Recomendação de parada.
--
-- Modelo: um pneu com @age@ voltas de uso custa aproximadamente @degRate * age@
-- segundos por volta em relação a um pneu novo. Mantê-lo pelas @r@ voltas
-- restantes custa portanto @degRate * age * r@ segundos a mais do que trocar
-- agora, ao preço único de @pitLaneLossS@. A parada passa a compensar quando a
-- idade do pneu atinge @pitLaneLossS / (degRate * r)@.
recommendNextPitStop :: TyreCompound -> Int -> Double -> Int -> Int -> Maybe StrategyRecommendation
recommendNextPitStop compound tyreAge degRate currentLap totalLaps
  | remainingLaps <= 0 = Nothing
  | degRate <= 0.0 = Nothing
  | targetLapNumber > totalLaps = Nothing
  | otherwise =
      Just
        StrategyRecommendation
          { targetLap = targetLapNumber
          , nextCompound = successor
          , projectedDeltaGainS = projectedGain
          , rationalBasis =
              T.concat
                [ "Degradação medida de "
                , formatFixed 3 degRate
                , " s/volta sobre "
                , renderCompound compound
                , " com "
                , T.pack (show tyreAge)
                , " voltas de uso. Faltam "
                , T.pack (show remainingLaps)
                , " voltas: manter o pneu custa "
                , formatFixed 1 (degRate * fromIntegral tyreAge * fromIntegral remainingLaps)
                , " s contra "
                , formatFixed 1 pitLaneLossS
                , " s da passagem pelos boxes."
                ]
          , recConfidence = mkConfidence (min 0.95 (0.55 + degRate * 3.0))
          }
  where
    remainingLaps = totalLaps - currentLap
    -- Idade em que a perda acumulada iguala o custo da passagem pelos boxes.
    breakEvenAge = pitLaneLossS / (degRate * fromIntegral remainingLaps)
    lapsUntilWorthIt = max 0 (ceiling breakEvenAge - tyreAge)
    targetLapNumber = currentLap + max 1 lapsUntilWorthIt
    successor = case compound of
      Soft -> Medium
      Medium -> Hard
      Hard -> Medium
      Intermediate -> Wet
      Wet -> Intermediate
      UnknownCompound -> Medium
    projectedGain =
      degRate * fromIntegral tyreAge * fromIntegral remainingLaps - pitLaneLossS

-- | Analisa um stint real e devolve medição e previsão lado a lado.
analyseStint :: Maybe Double -> Maybe Int -> StintObservation -> StintAnalysis
analyseStint trackTemp totalLaps stint =
  StintAnalysis
    { anDriverNumber = obsDriverNumber stint
    , anStintNumber = obsStintNumber stint
    , anCompound = compound
    , anLapStart = if null allLaps then 0 else minimum (map stintLapNumber allLaps)
    , anLapEnd = if null allLaps then 0 else maximum (map stintLapNumber allLaps)
    , anLapCount = length allLaps
    , anRepresentativeLaps = length usable
    , anAvgLapTimeS = if null usable then 0.0 else sum (map stintLapTimeS usable) / fromIntegral (length usable)
    , anBestLapTimeS = if null usable then 0.0 else minimum (map stintLapTimeS usable)
    , anObservedDegradationSPerLap = observed
    , anPredictedPaceLossS = predictTyrePaceLoss compound finalAge temperature
    , anCliffLap = tyreCliffLap compound
    , anInCliff = isTyreInCliff compound finalAge
    , anRecommendation =
        case totalLaps of
          Nothing -> Nothing
          Just total -> recommendNextPitStop compound finalAge observed currentLap total
    , anNotes = notes
    }
  where
    compound = obsCompound stint
    allLaps = obsLaps stint
    usable = representativeLaps allLaps
    observed = linearDegradation [(fromIntegral (stintLapTyreAge l), stintLapTimeS l) | l <- usable]
    finalAge = maximum (0 : map stintLapTyreAge allLaps)
    currentLap = maximum (0 : map stintLapNumber allLaps)
    temperature = fromMaybe 30.0 trackTemp
    notes =
      concat
        [ [ "Menos de 3 voltas representativas: a inclinação de degradação não é estatisticamente utilizável."
          | length usable < 3
          ]
        , [ "Temperatura de pista ausente na sessão; o modelo térmico usou 30 °C como referência."
          | Nothing <- [trackTemp]
          ]
        , [ T.concat
              [ "Pneu além do cliff nominal de "
              , T.pack (show (tyreCliffLap compound))
              , " voltas para o composto "
              , renderCompound compound
              , "."
              ]
          | isTyreInCliff compound finalAge
          ]
        , [ "Degradação medida negativa: a pista ainda estava evoluindo e os tempos melhoraram ao longo do stint."
          | observed < -0.005
          ]
        ]

analyseStints :: DegradationRequest -> [StintAnalysis]
analyseStints request =
  map (analyseStint (degTrackTemperatureC request) (degTotalSessionLaps request)) (degStints request)
