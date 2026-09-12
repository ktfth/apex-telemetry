{-# LANGUAGE OverloadedStrings #-}

-- | Servidor HTTP do motor de domínio.
--
-- É consumido pelo gateway C++23, que envia evidência já medida e recebe de
-- volta a narrativa explicável e a análise de degradação. O serviço é puro em
-- relação aos dados: não busca telemetria, não guarda estado e não completa
-- entrada ausente com valores plausíveis.
module Apex.Api.Server (runServer, application, handleRequest) where

import Apex.Api.Json
import Apex.Domain.Degradation (analyseStints)
import Apex.Domain.Rules (generateInsights)
import Data.Aeson (encode, object, (.=))
import qualified Data.ByteString.Lazy as BL
import Data.Text (Text)
import qualified Data.Text as T
import Network.HTTP.Types
  ( Method
  , Status
  , methodGet
  , methodPost
  , status200
  , status400
  , status404
  , status405
  )
import Network.Wai
import qualified Network.Wai.Handler.Warp as Warp

jsonResponse :: Status -> BL.ByteString -> Response
jsonResponse status =
  responseLBS
    status
    [ ("Content-Type", "application/json; charset=utf-8")
    , ("X-Apex-Engine", "strategy-hs")
    ]

healthPayload :: BL.ByteString
healthPayload =
  encode $
    object
      [ "status" .= ("healthy" :: Text)
      , "service" .= engineName
      , "version" .= engineVersion
      , "endpoints" .= (["POST /v1/insights", "POST /v1/degradation"] :: [Text])
      ]

handleRequest :: Method -> [Text] -> BL.ByteString -> Response
handleRequest method path body
  | method == methodGet && isHealthPath = jsonResponse status200 healthPayload
  | method /= methodPost =
      jsonResponse
        status405
        (encodeError "METHOD_NOT_ALLOWED" "Only GET /health and POST /v1/* are served")
  | path == ["v1", "insights"] =
      case decodeInsightRequest body of
        Left err -> jsonResponse status400 (encodeError "INVALID_EVIDENCE" (T.pack err))
        Right request -> jsonResponse status200 (encodeInsightResponse (generateInsights request))
  | path == ["v1", "degradation"] =
      case decodeDegradationRequest body of
        Left err -> jsonResponse status400 (encodeError "INVALID_STINTS" (T.pack err))
        Right request -> jsonResponse status200 (encodeDegradationResponse (analyseStints request))
  | otherwise = jsonResponse status404 (encodeError "NOT_FOUND" "Unknown endpoint")
  where
    isHealthPath = path `elem` [[], ["health"], ["v1", "health"]]

application :: Application
application request respond = do
  body <- strictRequestBody request
  respond (handleRequest (requestMethod request) (pathInfo request) body)

runServer :: Int -> IO ()
runServer port = do
  putStrLn ("[apex-strategy-hs] listening on http://0.0.0.0:" ++ show port)
  Warp.run port application
