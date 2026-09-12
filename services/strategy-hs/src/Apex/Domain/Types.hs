{-# LANGUAGE DeriveGeneric #-}
{-# LANGUAGE OverloadedStrings #-}
{-# LANGUAGE StrictData #-}

-- | Tipos de domínio da ApexTelemetry.
--
-- Todos os valores aqui descrevem medições reais recebidas do motor de
-- alinhamento espacial em C++; nenhum construtor fabrica telemetria.
module Apex.Domain.Types where

import Data.Text (Text)
import qualified Data.Text as T
import GHC.Generics (Generic)

-- | Composto oficial FIA/Pirelli.
data TyreCompound
  = Soft
  | Medium
  | Hard
  | Intermediate
  | Wet
  | UnknownCompound
  deriving (Show, Eq, Ord, Generic)

parseCompound :: Text -> TyreCompound
parseCompound raw =
  case T.toUpper (T.strip raw) of
    "SOFT" -> Soft
    "MEDIUM" -> Medium
    "HARD" -> Hard
    "INTERMEDIATE" -> Intermediate
    "WET" -> Wet
    _ -> UnknownCompound

renderCompound :: TyreCompound -> Text
renderCompound Soft = "SOFT"
renderCompound Medium = "MEDIUM"
renderCompound Hard = "HARD"
renderCompound Intermediate = "INTERMEDIATE"
renderCompound Wet = "WET"
renderCompound UnknownCompound = "UNKNOWN"

-- | Classificação categórica da volta.
data LapKind
  = FlyingLap
  | OutLap
  | InLap
  | InvalidLap Text
  | SafetyCarAffected
  deriving (Show, Eq, Generic)

-- | Volta normalizada para análise de ritmo.
data Lap = Lap
  { lapNumber :: Int
  , lapTimeS :: Double
  , lapKind :: LapKind
  , lapCompound :: TyreCompound
  , stintNumber :: Int
  , tyreAgeLaps :: Int
  , coveragePct :: Double
  }
  deriving (Show, Eq, Generic)

-- | Causa dominante da perda de tempo, determinada pelo motor numérico.
data LossCause
  = ApexSpeed
  | ThrottleApplication
  | BrakingPoint
  | TopSpeed
  | MixedCause
  deriving (Show, Eq, Generic)

parseLossCause :: Text -> LossCause
parseLossCause raw =
  case T.toUpper (T.strip raw) of
    "APEX_SPEED" -> ApexSpeed
    "THROTTLE_APPLICATION" -> ThrottleApplication
    "BRAKING_POINT" -> BrakingPoint
    "TOP_SPEED" -> TopSpeed
    _ -> MixedCause

renderLossCause :: LossCause -> Text
renderLossCause ApexSpeed = "APEX_SPEED"
renderLossCause ThrottleApplication = "THROTTLE_APPLICATION"
renderLossCause BrakingPoint = "BRAKING_POINT"
renderLossCause TopSpeed = "TOP_SPEED"
renderLossCause MixedCause = "MIXED"

-- | Evidência medida em um trecho da volta.
data Segment = Segment
  { segmentId :: Text
  , cornerLabel :: Text
  , distanceStartM :: Double
  , distanceEndM :: Double
  , apexDistanceM :: Double
  , timeLossS :: Double
  , minSpeedRefKmh :: Double
  , minSpeedCompKmh :: Double
  , maxSpeedRefKmh :: Double
  , maxSpeedCompKmh :: Double
  , fullThrottleDistRefM :: Double
  , fullThrottleDistCompM :: Double
  , brakingPointRefM :: Double
  , brakingPointCompM :: Double
  , brakingPointDiffM :: Double
  , segmentIsCorner :: Bool
  , segmentCause :: LossCause
  , segmentConfidence :: Double
  }
  deriving (Show, Eq, Generic)

-- | Cabeçalho de uma volta comparada.
data LapRef = LapRef
  { refDriverNumber :: Int
  , refDriverCode :: Text
  , refLapNumber :: Int
  , refLapTimeS :: Double
  , refCompound :: TyreCompound
  , refTyreAgeLaps :: Int
  }
  deriving (Show, Eq, Generic)

-- | Contexto da sessão em que a comparação ocorreu.
data SessionContext = SessionContext
  { sessionKey :: Int
  , circuitName :: Text
  , sessionName :: Text
  , trackTemperatureC :: Maybe Double
  }
  deriving (Show, Eq, Generic)

-- | Pedido completo de explicabilidade.
data InsightRequest = InsightRequest
  { requestSession :: SessionContext
  , requestReference :: LapRef
  , requestComparison :: LapRef
  , requestSegments :: [Segment]
  }
  deriving (Show, Eq, Generic)

-- | Nível de confiança garantido no intervalo [0.0, 1.0].
newtype Confidence = Confidence Double
  deriving (Show, Eq, Ord, Generic)

mkConfidence :: Double -> Confidence
mkConfidence c = Confidence (max 0.0 (min 1.0 c))

confidenceValue :: Confidence -> Double
confidenceValue (Confidence c) = c

-- | Insight explicável, auditável e determinístico.
data Insight = Insight
  { insightId :: Text
  , driverCode :: Text
  , insightSegment :: Segment
  , explanation :: Text
  , assumptions :: [Text]
  , limitations :: [Text]
  , confidence :: Confidence
  }
  deriving (Show, Eq, Generic)

-- | Volta observada dentro de um stint, usada na regressão de degradação.
data StintLap = StintLap
  { stintLapNumber :: Int
  , stintLapTimeS :: Double
  , stintLapTyreAge :: Int
  , stintLapValid :: Bool
  }
  deriving (Show, Eq, Generic)

-- | Stint real medido na sessão.
data StintObservation = StintObservation
  { obsDriverNumber :: Int
  , obsStintNumber :: Int
  , obsCompound :: TyreCompound
  , obsTyreAgeAtStart :: Int
  , obsLaps :: [StintLap]
  }
  deriving (Show, Eq, Generic)

data DegradationRequest = DegradationRequest
  { degTrackTemperatureC :: Maybe Double
  , degTotalSessionLaps :: Maybe Int
  , degStints :: [StintObservation]
  }
  deriving (Show, Eq, Generic)

-- | Recomendação determinística de troca de pneu.
data StrategyRecommendation = StrategyRecommendation
  { targetLap :: Int
  , nextCompound :: TyreCompound
  , projectedDeltaGainS :: Double
  , rationalBasis :: Text
  , recConfidence :: Confidence
  }
  deriving (Show, Eq, Generic)

-- | Resultado da análise de um stint.
data StintAnalysis = StintAnalysis
  { anDriverNumber :: Int
  , anStintNumber :: Int
  , anCompound :: TyreCompound
  , anLapStart :: Int
  , anLapEnd :: Int
  , anLapCount :: Int
  , anRepresentativeLaps :: Int
  , anAvgLapTimeS :: Double
  , anBestLapTimeS :: Double
  , anObservedDegradationSPerLap :: Double
  , anPredictedPaceLossS :: Double
  , anCliffLap :: Int
  , anInCliff :: Bool
  , anRecommendation :: Maybe StrategyRecommendation
  , anNotes :: [Text]
  }
  deriving (Show, Eq, Generic)
