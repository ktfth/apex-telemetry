{-# LANGUAGE DeriveGeneric #-}
{-# LANGUAGE StrictData #-}
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
  deriving (Show, Eq, Generic)

-- | Classificação categórica de voltas
data LapKind
  = FlyingLap
  | OutLap
  | InLap
  | InvalidLap !Text
  | SafetyCarAffected
  deriving (Show, Eq, Generic)

-- | Evidência empírica observada no trecho de telemetria
data Evidence = Evidence
  { distanceStartM          :: !Double
  , distanceEndM            :: !Double
  , timeDeltaS              :: !Double
  , minSpeedRefKmh          :: !Double
  , minSpeedCompKmh         :: !Double
  , fullThrottleDistRefM    :: !Double
  , fullThrottleDistCompM   :: !Double
  , brakingPointDiffM       :: !Double
  } deriving (Show, Eq, Generic)

-- | Nível de confiança com limite estrito [0.0, 1.0]
newtype Confidence = Confidence Double
  deriving (Show, Eq, Ord, Generic)

-- | Insight explicável estritamente auditável
data Insight = Insight
  { insightId      :: !Text
  , driverCode     :: !Text
  , evidence       :: !Evidence
  , explanation    :: !Text
  , assumptions    :: ![Text]
  , limitations    :: ![Text]
  , confidence     :: !Confidence
  } deriving (Show, Eq, Generic)

-- | Stint com pneu e ciclo de vida
data Stint = Stint
  { stintNumber       :: !Int
  , compound          :: !TyreCompound
  , lapStart          :: !Int
  , lapEnd            :: !Int
  , avgLapTimeS       :: !Double
  , degradationRate   :: !Double -- Segundos por volta
  } deriving (Show, Eq, Generic)

-- | Evento de direção de prova
data RaceControlEvent = RaceControlEvent
  { occurredAt     :: !UTCTime
  , category       :: !Text
  , flag           :: !(Maybe Text)
  , message        :: !Text
  , sector         :: !(Maybe Int)
  , driverNumber   :: !(Maybe Int)
  } deriving (Show, Eq, Generic)
