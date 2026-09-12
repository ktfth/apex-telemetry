module Main (main) where

import Apex.Api.Server (runServer)
import System.Environment (lookupEnv)
import System.IO (BufferMode (LineBuffering), hSetBuffering, stderr, stdout)
import Text.Read (readMaybe)

-- | Porta padrão do motor de estratégia; o gateway usa a mesma convenção em
-- `APEX_STRATEGY_URL`.
defaultPort :: Int
defaultPort = 8092

main :: IO ()
main = do
  -- Redirecionada para arquivo, a saída vira bloco-bufferizada e o log só aparece
  -- quando o processo morre; por linha, ele serve para diagnóstico ao vivo.
  hSetBuffering stdout LineBuffering
  hSetBuffering stderr LineBuffering
  configured <- lookupEnv "APEX_STRATEGY_PORT"
  let port = maybe defaultPort id (configured >>= readMaybe)
  runServer port
