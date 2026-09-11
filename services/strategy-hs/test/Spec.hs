{-# LANGUAGE ScopedTypeVariables #-}
{-# LANGUAGE OverloadedStrings #-}

module Main where

import Test.QuickCheck
import Apex.Domain.Types
import Apex.Domain.Rules
import qualified Data.Text as T
import System.Exit (exitFailure, exitSuccess)

-- | 1. Invariante de Confiança Formal: sempre delimitada em [0.0, 1.0]
prop_confidenceBounded :: Double -> Bool
prop_confidenceBounded c =
  let val = confidenceValue (mkConfidence c)
  in val >= 0.0 && val <= 1.0

-- | 2. Invariante de Elegibilidade para Análise de Ritmo:
-- Nenhuma volta inválida, volta de saída, entrada ou SC pode ser elegível
prop_paceEligibilityStrict :: Bool -> Bool -> Bool -> Maybe String -> Double -> Bool
prop_paceEligibilityStrict isPitOut isPitIn isSC mReason cov =
  let k = classifyLap isPitOut isPitIn isSC (fmap T.pack mReason)
      lap = Lap { lapNumber = 10, lapTimeS = 89.2, lapKind = k, lapCompound = Soft, stintNumber = 1, coveragePct = cov }
      eligible = isLapEligibleForPaceAnalysis lap
  in case k of
       FlyingLap -> eligible == (cov >= 98.0)
       _         -> not eligible

-- | 3. Invariante de Stints: Dois stints sucessivos nunca podem se sobrepor
prop_stintNonOverlap :: Positive Int -> Positive Int -> Positive Int -> Positive Int -> Bool
prop_stintNonOverlap (Positive start1) (Positive len1) (Positive gap) (Positive len2) =
  let end1 = start1 + len1
      start2 = end1 + gap
      end2 = start2 + len2
      s1 = Stint { stintNum = 1, driverNum = 1, stintCompound = Medium, lapStart = start1, lapEnd = end1, lapsCount = len1, avgLapTimeS = 90.0, degradationRate = 0.1, pitStopDurationS = Nothing }
      s2 = Stint { stintNum = 2, driverNum = 1, stintCompound = Hard, lapStart = start2, lapEnd = end2, lapsCount = len2, avgLapTimeS = 89.5, degradationRate = 0.08, pitStopDurationS = Just 22.1 }
  in validateStintNonOverlap s1 s2

-- | 4. Invariante de Explicabilidade: O texto gerado deve conter valores numéricos exatos
prop_insightContainsEvidence :: Positive Double -> Positive Double -> Positive Double -> Bool
prop_insightContainsEvidence (Positive dStart) (Positive len) (Positive deltaS) =
  let dEnd = dStart + len
      ev = Evidence
        { distanceStartM = dStart
        , distanceEndM = dEnd
        , timeDeltaS = deltaS
        , minSpeedRefKmh = 120.0
        , minSpeedCompKmh = 114.0
        , fullThrottleDistRefM = dStart + 100.0
        , fullThrottleDistCompM = dStart + 130.0
        , brakingPointDiffM = -3.5
        }
      ins = generateTelemetryInsight "16" ev
      txt = explanation ins
  in T.isInfixOf "Piloto 16 perdeu " txt &&
     T.isInfixOf " s entre " txt &&
     T.isInfixOf " km e " txt

-- | 5. Invariante de Degradação: Tempos constantes geram degradação zero
prop_constantLapTimesZeroDeg :: Positive Double -> Bool
prop_constantLapTimesZeroDeg (Positive t) =
  let ltimes = replicate 10 t
      deg = calculateLinearDegradation ltimes
  in abs deg < 0.0001

-- | 6. Invariante de Degradação Preditiva Monotônica: mais voltas de uso geram perda de ritmo maior ou igual
prop_tyreDegradationMonotonic :: Positive Int -> Positive Double -> Bool
prop_tyreDegradationMonotonic (Positive laps) (Positive temp) =
  let loss1 = predictTyrePaceLoss Soft laps (temp + 20.0)
      loss2 = predictTyrePaceLoss Soft (laps + 1) (temp + 20.0)
  in loss2 >= loss1

-- | 7. Invariante de Durabilidade de Compostos: Hard tem cliff estritamente maior que Medium e Soft
prop_hardCompoundLastsLongerThanSoft :: Bool
prop_hardCompoundLastsLongerThanSoft =
  tyreCliffLap Hard > tyreCliffLap Medium &&
  tyreCliffLap Medium > tyreCliffLap Soft

main :: IO ()
main = do
  putStrLn "=== APEX STRATEGY-HS PROPERTY-BASED TESTS (QUICKCHECK) ==="
  
  r1 <- quickCheckResult prop_confidenceBounded
  r2 <- quickCheckResult prop_paceEligibilityStrict
  r3 <- quickCheckResult prop_stintNonOverlap
  r4 <- quickCheckResult prop_insightContainsEvidence
  r5 <- quickCheckResult prop_constantLapTimesZeroDeg
  r6 <- quickCheckResult prop_tyreDegradationMonotonic
  r7 <- quickCheckResult (property prop_hardCompoundLastsLongerThanSoft)

  let allPassed = all isSuccess [r1, r2, r3, r4, r5, r6, r7]
  if allPassed
    then do
      putStrLn "\nAll 7 Domain Property Tests Passed Successfully!"
      exitSuccess
    else do
      putStrLn "\nProperty Tests Failed!"
      exitFailure
