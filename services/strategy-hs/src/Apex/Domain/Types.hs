{-# LANGUAGE DeriveGeneric #-}
{-# LANGUAGE StrictData #-}
{-# LANGUAGE OverloadedStrings #-}

module Apex.Domain.Types where

import GHC.Generics (Generic)
import Data.Text (Text)
import Data.Time.Clock (UTCTime)

-- | Status da sessão de corrida ou qualificação
data SessionStatus
  = SessionScheduled
  | SessionLive
  | SessionYellowFlag
  | SessionRedFlag
  | SessionFinished
  deriving (Show, Eq, Generic)

-- | Composto de pneus oficial FIA / Pirelli
data TyreCompound
  = Soft
  | Medium
  | Hard
  | Intermediate
  | Wet
  | UnknownCompound
  deriving (Show, Eq, Ord, Generic)

-- | Classificação categórica e formal de voltas
data LapKind
  = FlyingLap
  | OutLap
  | InLap
  | InvalidLap !Text
  | SafetyCarAffected
  deriving (Show, Eq, Generic)

-- | Volta normalizada para análise de domínio
data Lap = Lap
  { lapNumber    :: !Int
  , lapTimeS     :: !Double
  , lapKind      :: !LapKind
  , lapCompound  :: !TyreCompound
  , stintNumber  :: !Int
  , coveragePct  :: !Double
  } deriving (Show, Eq, Generic)

-- | Evidência empírica observada no trecho de telemetria
data Evidence = Evidence
  { distanceStartM        :: !Double
  , distanceEndM          :: !Double
  , timeDeltaS            :: !Double
  , minSpeedRefKmh        :: !Double
  , minSpeedCompKmh       :: !Double
  , fullThrottleDistRefM  :: !Double
  , fullThrottleDistCompM :: !Double
  , brakingPointDiffM     :: !Double
  } deriving (Show, Eq, Generic)

-- | Nível de confiança garantido no intervalo [0.0, 1.0]
newtype Confidence = Confidence Double
  deriving (Show, Eq, Ord, Generic)

mkConfidence :: Double -> Confidence
mkConfidence c = Confidence (max 0.0 (min 1.0 c))

confidenceValue :: Confidence -> Double
confidenceValue (Confidence c) = c

-- | Insight explicável auditável e estritamente determinístico
data Insight = Insight
  { insightId     :: !Text
  , driverCode    :: !Text
  , evidence      :: !Evidence
  , explanation   :: !Text
  , assumptions   :: ![Text]
  , limitations   :: ![Text]
  , confidence    :: !Confidence
  } deriving (Show, Eq, Generic)

-- | Stint com pneu e ciclo de vida
data Stint = Stint
  { stintNum          :: !Int
  , driverNum         :: !Int
  , stintCompound     :: !TyreCompound
  , lapStart          :: !Int
  , lapEnd            :: !Int
  , lapsCount         :: !Int
  , avgLapTimeS       :: !Double
  , degradationRate   :: !Double -- Segundos perdidos por volta
  , pitStopDurationS  :: !(Maybe Double)
  } deriving (Show, Eq, Generic)

-- | Estado de telemetria do piloto
data DriverState = DriverState
  { currentDriverNum :: !Int
  , currentStintNum  :: !Int
  , activeCompound   :: !TyreCompound
  , tyreAgeLaps      :: !Int
  , inPitLane        :: !Bool
  } deriving (Show, Eq, Generic)

-- | Recomendação estratégica pura
data StrategyRecommendation = StrategyRecommendation
  { targetLap             :: !Int
  , nextCompound          :: !TyreCompound
  , projectedDeltaGainS   :: !Double
  , rationalBasis         :: !Text
  , recConfidence         :: !Confidence
  } deriving (Show, Eq, Generic)

-- | Evento oficial de direção de prova
data RaceControlEvent = RaceControlEvent
  { occurredAt     :: !UTCTime
  , category       :: !Text
  , flag           :: !(Maybe Text)
  , eventMessage   :: !Text
  , sector         :: !(Maybe Int)
  , eventDriver    :: !(Maybe Int)
  } deriving (Show, Eq, Generic)
