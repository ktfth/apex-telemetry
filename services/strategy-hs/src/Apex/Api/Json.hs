{-# LANGUAGE OverloadedStrings #-}
{-# OPTIONS_GHC -Wno-orphans #-}

-- | Contrato de fio entre o gateway C++23 e o motor de domínio em Haskell.
--
-- A decodificação é estrita quanto aos campos obrigatórios: um pedido sem a
-- evidência necessária é rejeitado com erro, nunca completado com valores
-- plausíveis inventados.
module Apex.Api.Json
  ( decodeInsightRequest
  , encodeInsightResponse
  , decodeDegradationRequest
  , encodeDegradationResponse
  , encodeError
  , engineName
  , engineVersion
  ) where

import Apex.Domain.Types
import Data.Aeson
import Data.Aeson.Types (Parser)
import qualified Data.ByteString.Lazy as BL
import Data.Text (Text)

engineName :: Text
engineName = "apex-strategy-hs"

engineVersion :: Text
engineVersion = "2.0.0"

instance FromJSON SessionContext where
  parseJSON = withObject "session" $ \o ->
    SessionContext
      <$> o .:? "session_key" .!= 0
      <*> o .:? "circuit_name" .!= "unknown"
      <*> o .:? "session_name" .!= "unknown"
      <*> o .:? "track_temperature_c"

parseLapRef :: Value -> Parser LapRef
parseLapRef = withObject "lap" $ \o ->
  LapRef
    <$> o .: "driver_number"
    <*> o .:? "driver_code" .!= "?"
    <*> o .: "lap_number"
    <*> o .:? "lap_time_s" .!= 0.0
    <*> (parseCompound <$> o .:? "compound" .!= "UNKNOWN")
    <*> o .:? "tyre_age_laps" .!= 0

instance FromJSON LapRef where
  parseJSON = parseLapRef

instance FromJSON Segment where
  parseJSON = withObject "segment" $ \o ->
    Segment
      <$> o .: "id"
      <*> o .:? "corner_label" .!= ""
      <*> o .: "distance_start_m"
      <*> o .: "distance_end_m"
      <*> o .:? "apex_distance_m" .!= 0.0
      <*> o .: "time_loss_s"
      <*> o .:? "min_speed_ref_kmh" .!= 0.0
      <*> o .:? "min_speed_comp_kmh" .!= 0.0
      <*> o .:? "max_speed_ref_kmh" .!= 0.0
      <*> o .:? "max_speed_comp_kmh" .!= 0.0
      <*> o .:? "full_throttle_distance_ref_m" .!= 0.0
      <*> o .:? "full_throttle_distance_comp_m" .!= 0.0
      <*> o .:? "braking_point_ref_m" .!= 0.0
      <*> o .:? "braking_point_comp_m" .!= 0.0
      <*> o .:? "braking_point_diff_m" .!= 0.0
      <*> o .:? "is_corner" .!= True
      <*> (parseLossCause <$> o .:? "cause" .!= "MIXED")
      <*> o .:? "confidence" .!= 0.8

instance FromJSON InsightRequest where
  parseJSON = withObject "insight_request" $ \o ->
    InsightRequest
      <$> o .:? "session" .!= SessionContext 0 "unknown" "unknown" Nothing
      <*> o .: "reference"
      <*> o .: "comparison"
      <*> o .:? "segments" .!= []

instance ToJSON Segment where
  toJSON s =
    object
      [ "id" .= segmentId s
      , "corner_label" .= cornerLabel s
      , "distance_start_m" .= distanceStartM s
      , "distance_end_m" .= distanceEndM s
      , "apex_distance_m" .= apexDistanceM s
      , "time_loss_s" .= timeLossS s
      , "min_speed_ref_kmh" .= minSpeedRefKmh s
      , "min_speed_comp_kmh" .= minSpeedCompKmh s
      , "max_speed_ref_kmh" .= maxSpeedRefKmh s
      , "max_speed_comp_kmh" .= maxSpeedCompKmh s
      , "full_throttle_distance_ref_m" .= fullThrottleDistRefM s
      , "full_throttle_distance_comp_m" .= fullThrottleDistCompM s
      , "braking_point_ref_m" .= brakingPointRefM s
      , "braking_point_comp_m" .= brakingPointCompM s
      , "braking_point_diff_m" .= brakingPointDiffM s
      , "is_corner" .= segmentIsCorner s
      , "cause" .= renderLossCause (segmentCause s)
      , "corner" .= cornerLabel s
      ]

instance ToJSON Insight where
  toJSON i =
    object
      [ "id" .= insightId i
      , "driver_code" .= driverCode i
      , "distance_start_m" .= distanceStartM (insightSegment i)
      , "distance_end_m" .= distanceEndM (insightSegment i)
      , "time_loss_s" .= timeLossS (insightSegment i)
      , "summary" .= explanation i
      , "evidence" .= insightSegment i
      , "assumptions" .= assumptions i
      , "limitations" .= limitations i
      , "engine" .= engineName
      , "confidence" .= confidenceValue (confidence i)
      ]

instance FromJSON StintLap where
  parseJSON = withObject "stint_lap" $ \o ->
    StintLap
      <$> o .: "lap_number"
      <*> o .: "lap_time_s"
      <*> o .:? "tyre_age_laps" .!= 0
      <*> o .:? "is_valid" .!= True

instance FromJSON StintObservation where
  parseJSON = withObject "stint" $ \o ->
    StintObservation
      <$> o .: "driver_number"
      <*> o .: "stint_number"
      <*> (parseCompound <$> o .:? "compound" .!= "UNKNOWN")
      <*> o .:? "tyre_age_at_start" .!= 0
      <*> o .:? "laps" .!= []

instance FromJSON DegradationRequest where
  parseJSON = withObject "degradation_request" $ \o ->
    DegradationRequest
      <$> o .:? "track_temperature_c"
      <*> o .:? "total_session_laps"
      <*> o .:? "stints" .!= []

instance ToJSON StrategyRecommendation where
  toJSON r =
    object
      [ "target_lap" .= targetLap r
      , "next_compound" .= renderCompound (nextCompound r)
      , "projected_gain_s" .= projectedDeltaGainS r
      , "rationale" .= rationalBasis r
      , "confidence" .= confidenceValue (recConfidence r)
      ]

instance ToJSON StintAnalysis where
  toJSON a =
    object
      [ "driver_number" .= anDriverNumber a
      , "stint_number" .= anStintNumber a
      , "compound" .= renderCompound (anCompound a)
      , "lap_start" .= anLapStart a
      , "lap_end" .= anLapEnd a
      , "lap_count" .= anLapCount a
      , "representative_laps" .= anRepresentativeLaps a
      , "avg_lap_time_s" .= anAvgLapTimeS a
      , "best_lap_time_s" .= anBestLapTimeS a
      , "observed_degradation_s_per_lap" .= anObservedDegradationSPerLap a
      , "predicted_pace_loss_s" .= anPredictedPaceLossS a
      , "cliff_lap" .= anCliffLap a
      , "in_cliff" .= anInCliff a
      , "recommendation" .= anRecommendation a
      , "notes" .= anNotes a
      ]

decodeInsightRequest :: BL.ByteString -> Either String InsightRequest
decodeInsightRequest = eitherDecode'

decodeDegradationRequest :: BL.ByteString -> Either String DegradationRequest
decodeDegradationRequest = eitherDecode'

encodeInsightResponse :: [Insight] -> BL.ByteString
encodeInsightResponse insights =
  encode $
    object ["engine" .= engineName, "version" .= engineVersion, "insights" .= insights]

encodeDegradationResponse :: [StintAnalysis] -> BL.ByteString
encodeDegradationResponse analyses =
  encode $
    object ["engine" .= engineName, "version" .= engineVersion, "stints" .= analyses]

encodeError :: Text -> Text -> BL.ByteString
encodeError code message =
  encode $ object ["error" .= message, "code" .= code, "engine" .= engineName]
