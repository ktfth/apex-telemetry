module Main (main) where

import Apex.Api.Server (runServer)
import System.Environment (lookupEnv)
import Text.Read (readMaybe)

-- | Porta padrão do motor de estratégia; o gateway usa a mesma convenção em
-- `APEX_STRATEGY_URL`.
defaultPort :: Int
defaultPort = 8092

main :: IO ()
main = do
  configured <- lookupEnv "APEX_STRATEGY_PORT"
  let port = maybe defaultPort id (configured >>= readMaybe)
  runServer port
