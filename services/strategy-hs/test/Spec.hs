{-# LANGUAGE OverloadedStrings #-}
{-# LANGUAGE ScopedTypeVariables #-}

module Main (main) where

import Apex.Api.Json
import Apex.Domain.Degradation
import Apex.Domain.Rules
import Apex.Domain.Types
import qualified Data.ByteString.Lazy.Char8 as BL
import qualified Data.Text as T
import System.Exit (exitFailure, exitSuccess)
import Test.QuickCheck

-- Geradores sobre valores fisicamente plausíveis de telemetria de F1.

compounds :: [TyreCompound]
compounds = [Soft, Medium, Hard, Intermediate, Wet, UnknownCompound]

instance Arbitrary TyreCompound where
  arbitrary = elements compounds

instance Arbitrary LossCause where
  arbitrary = elements [ApexSpeed, ThrottleApplication, BrakingPoint, TopSpeed, MixedCause]

genSegment :: Gen Segment
genSegment = do
  start <- choose (0.0, 5000.0)
  len <- choose (80.0, 400.0)
  loss <- choose (-0.5, 0.5)
  minRef <- choose (60.0, 280.0)
  deficit <- choose (-8.0, 8.0)
  maxRef <- choose (200.0, 340.0)
  topDeficit <- choose (-10.0, 10.0)
  brakeRef <- choose (start, start + len)
  brakeDiff <- choose (-25.0, 25.0)
  cause <- arbitrary
  isCorner <- arbitrary
  conf <- choose (0.0, 1.0)
  pure
    Segment
      { segmentId = T.pack ("seg-" ++ show (round start :: Int))
      , cornerLabel = "T1"
      , distanceStartM = start
      , distanceEndM = start + len
      , apexDistanceM = start + len / 2
      , timeLossS = loss
      , minSpeedRefKmh = minRef
      , minSpeedCompKmh = minRef - deficit
      , maxSpeedRefKmh = maxRef
      , maxSpeedCompKmh = maxRef - topDeficit
      , fullThrottleDistRefM = start + len * 0.6
      , fullThrottleDistCompM = start + len * 0.6 + 15.0
      , brakingPointRefM = brakeRef
      , brakingPointCompM = brakeRef + brakeDiff
      , brakingPointDiffM = brakeDiff
      , segmentIsCorner = isCorner
      , segmentCause = cause
      , segmentConfidence = conf
      }

genRequest :: Gen InsightRequest
genRequest = do
  segments <- listOf1 genSegment
  refCompoundValue <- arbitrary
  compCompoundValue <- arbitrary
  refAge <- choose (0, 30)
  compAge <- choose (0, 30)
  temperature <- oneof [pure Nothing, Just <$> choose (10.0, 55.0)]
  pure
    InsightRequest
      { requestSession = SessionContext 9468 "Sakhir" "Qualifying" temperature
      , requestReference = LapRef 1 "VER" 16 89.179 refCompoundValue refAge
      , requestComparison = LapRef 16 "LEC" 15 89.407 compCompoundValue compAge
      , requestSegments = segments
      }

genStintLap :: Int -> Double -> Gen StintLap
genStintLap lapNumberValue baseTime = do
  noise <- choose (-0.15, 0.15)
  pure (StintLap lapNumberValue (baseTime + noise) lapNumberValue True)

-- 1. Confiança sempre no intervalo fechado [0, 1].
prop_confidenceBounded :: Double -> Bool
prop_confidenceBounded c =
  let value = confidenceValue (mkConfidence c)
   in value >= 0.0 && value <= 1.0

-- 2. Um insight nunca ultrapassa a confiança do trecho que o originou.
prop_insightConfidenceNeverExceedsSegment :: Property
prop_insightConfidenceNeverExceedsSegment =
  forAll genRequest $ \request ->
    all
      (\i -> confidenceValue (confidence i) <= segmentConfidence (insightSegment i) + 1e-9)
      (generateInsights request)

-- 3. Cada insight cita, no texto, os limites reais do trecho analisado.
prop_insightQuotesMeasuredDistances :: Property
prop_insightQuotesMeasuredDistances =
  forAll genRequest $ \request ->
    all
      (\i ->
         let text = explanation i
             segment = insightSegment i
          in T.isInfixOf (formatFixed 0 (distanceStartM segment)) text
               && T.isInfixOf (formatFixed 0 (distanceEndM segment)) text
               && T.isInfixOf (formatFixed 3 (timeLossS segment)) text)
      (generateInsights request)

-- 4. Compostos diferentes entre as voltas são sempre declarados como limitação.
prop_differentCompoundsAlwaysDisclosed :: Property
prop_differentCompoundsAlwaysDisclosed =
  forAll genRequest $ \request ->
    refCompound (requestReference request) /= refCompound (requestComparison request)
      ==> all (not . null . limitations) (generateInsights request)

-- 5. Tempos de volta constantes produzem degradação medida nula.
prop_constantLapTimesZeroDegradation :: Positive Double -> Bool
prop_constantLapTimesZeroDegradation (Positive t) =
  abs (linearDegradation [(fromIntegral age, t) | age <- [1 :: Int .. 10]]) < 1e-9

-- 6. Tempos crescentes produzem degradação medida positiva.
prop_increasingLapTimesPositiveDegradation :: Positive Double -> Property
prop_increasingLapTimesPositiveDegradation (Positive slope) =
  slope > 0.001
    ==> linearDegradation [(fromIntegral age, 90.0 + slope * fromIntegral age) | age <- [1 :: Int .. 10]]
      > 0.0

-- 7. O modelo térmico é monotônico na idade do pneu.
prop_tyreDegradationMonotonicInAge :: TyreCompound -> Positive Int -> Positive Double -> Bool
prop_tyreDegradationMonotonicInAge compound (Positive laps) (Positive temp) =
  predictTyrePaceLoss compound (laps + 1) temp >= predictTyrePaceLoss compound laps temp

-- 8. O modelo térmico é monotônico na temperatura de pista.
prop_tyreDegradationMonotonicInTemperature :: TyreCompound -> Positive Int -> Bool
prop_tyreDegradationMonotonicInTemperature compound (Positive laps) =
  predictTyrePaceLoss compound laps 45.0 >= predictTyrePaceLoss compound laps 25.0

-- 9. Compostos mais duros aguentam mais voltas antes do cliff.
prop_compoundDurabilityOrdering :: Bool
prop_compoundDurabilityOrdering =
  tyreCliffLap Hard > tyreCliffLap Medium && tyreCliffLap Medium > tyreCliffLap Soft

-- 10. Sem degradação medida não existe recomendação de parada.
prop_noDegradationNoPitRecommendation :: Positive Int -> Positive Int -> Bool
prop_noDegradationNoPitRecommendation (Positive age) (Positive remaining) =
  case recommendNextPitStop Soft age 0.0 10 (10 + remaining) of
    Nothing -> True
    Just _ -> False

-- 11. Outliers de tráfego não entram na regressão de degradação.
prop_outliersExcludedFromRegression :: Property
prop_outliersExcludedFromRegression =
  forAll (mapM (`genStintLap` 90.0) [1 .. 8]) $ \laps ->
    let poisoned = laps ++ [StintLap 9 130.0 9 True]
     in length (representativeLaps poisoned) == length (representativeLaps laps)

-- 12. Stints declarados com faixas de voltas disjuntas nunca se sobrepõem.
prop_stintNonOverlap :: Positive Int -> Positive Int -> Positive Int -> Positive Int -> Bool
prop_stintNonOverlap (Positive start1) (Positive len1) (Positive gap) (Positive len2) =
  let end1 = start1 + len1
      start2 = end1 + gap
      end2 = start2 + len2
      mk n a b = StintObservation 1 n Soft 0 [StintLap l 90.0 l True | l <- [a .. b]]
   in validateStintNonOverlap (mk 1 start1 end1) (mk 2 start2 end2)

-- 13. Voltas fora do regime de volta rápida nunca entram na análise de ritmo.
prop_paceEligibilityStrict :: Bool -> Bool -> Bool -> Maybe String -> Double -> Bool
prop_paceEligibilityStrict isPitOut isPitIn isSC reason coverage =
  let kind = classifyLap isPitOut isPitIn isSC (fmap T.pack reason)
      lap = Lap 10 89.2 kind Soft 1 3 coverage
   in case kind of
        FlyingLap -> isLapEligibleForPaceAnalysis lap == (coverage >= 98.0)
        _ -> not (isLapEligibleForPaceAnalysis lap)

-- 14. O contrato de fio sobrevive a uma ida e volta pelo decodificador.
prop_wireContractRoundTrip :: Bool
prop_wireContractRoundTrip =
  case decodeInsightRequest payload of
    Left _ -> False
    Right request ->
      length (requestSegments request) == 1
        && refDriverCode (requestComparison request) == "LEC"
        && not (null (generateInsights request))
  where
    payload =
      BL.pack
        "{\"session\":{\"session_key\":9468,\"circuit_name\":\"Sakhir\",\"session_name\":\"Qualifying\",\"track_temperature_c\":22.2},\
        \\"reference\":{\"driver_number\":1,\"driver_code\":\"VER\",\"lap_number\":16,\"lap_time_s\":89.179,\"compound\":\"SOFT\",\"tyre_age_laps\":0},\
        \\"comparison\":{\"driver_number\":16,\"driver_code\":\"LEC\",\"lap_number\":15,\"lap_time_s\":89.407,\"compound\":\"SOFT\",\"tyre_age_laps\":0},\
        \\"segments\":[{\"id\":\"seg-1420-1750\",\"corner_label\":\"T4\",\"distance_start_m\":1420,\"distance_end_m\":1750,\
        \\"apex_distance_m\":1550,\"time_loss_s\":0.228,\"min_speed_ref_kmh\":118.2,\"min_speed_comp_kmh\":111.4,\
        \\"max_speed_ref_kmh\":250,\"max_speed_comp_kmh\":248,\"full_throttle_distance_ref_m\":1550,\
        \\"full_throttle_distance_comp_m\":1581,\"braking_point_ref_m\":1440,\"braking_point_comp_m\":1436,\
        \\"braking_point_diff_m\":-4.2,\"cause\":\"APEX_SPEED\",\"confidence\":0.94}]}"

-- 15. Um pedido sem evidência obrigatória é rejeitado, não completado.
prop_incompleteRequestRejected :: Bool
prop_incompleteRequestRejected =
  case decodeInsightRequest (BL.pack "{\"segments\":[]}") of
    Left _ -> True
    Right _ -> False

main :: IO ()
main = do
  putStrLn "=== APEX STRATEGY-HS — PROPRIEDADES DE DOMÍNIO (QUICKCHECK) ==="
  results <-
    sequence
      [ quickCheckResult prop_confidenceBounded
      , quickCheckResult prop_insightConfidenceNeverExceedsSegment
      , quickCheckResult prop_insightQuotesMeasuredDistances
      , quickCheckResult prop_differentCompoundsAlwaysDisclosed
      , quickCheckResult prop_constantLapTimesZeroDegradation
      , quickCheckResult prop_increasingLapTimesPositiveDegradation
      , quickCheckResult prop_tyreDegradationMonotonicInAge
      , quickCheckResult prop_tyreDegradationMonotonicInTemperature
      , quickCheckResult (property prop_compoundDurabilityOrdering)
      , quickCheckResult prop_noDegradationNoPitRecommendation
      , quickCheckResult prop_outliersExcludedFromRegression
      , quickCheckResult prop_stintNonOverlap
      , quickCheckResult prop_paceEligibilityStrict
      , quickCheckResult (property prop_wireContractRoundTrip)
      , quickCheckResult (property prop_incompleteRequestRejected)
      ]
  if all isSuccess results
    then do
      putStrLn ("\n" ++ show (length results) ++ " propriedades de domínio validadas.")
      exitSuccess
    else do
      putStrLn "\nFalha nas propriedades de domínio."
      exitFailure
