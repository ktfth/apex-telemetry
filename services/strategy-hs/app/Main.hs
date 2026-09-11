{-# LANGUAGE OverloadedStrings #-}
module Main where

import Apex.Domain.Types
import Apex.Domain.Rules
import qualified Data.Text.IO as TIO

main :: IO ()
main = do
  let ev = Evidence
        { distanceStartM = 1420.0
        , distanceEndM = 2080.0
        , timeDeltaS = 0.24
        , minSpeedRefKmh = 118.2
        , minSpeedCompKmh = 111.4
        , fullThrottleDistRefM = 1640.0
        , fullThrottleDistCompM = 1671.0
        , brakingPointDiffM = -4.2
        }
      ins = buildExplainableLossInsight "16" ev
  putStrLn "{"
  putStrLn "  \"service\": \"apex-strategy-hs\","
  putStrLn "  \"version\": \"1.0.0\","
  putStrLn "  \"status\": \"initialized\","
  putStrLn "  \"sample_insight\": {"
  putStrLn $ "    \"driver\": \"" ++ show (driverCode ins) ++ "\","
  TIO.putStrLn $ "    \"explanation\": \"" <> explanation ins <> "\""
  putStrLn "  }"
  putStrLn "}"
