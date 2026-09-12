-- ApexTelemetry — esquema relacional e hypertables
-- PostgreSQL 16 + TimescaleDB
--
-- Todas as tabelas são preenchidas exclusivamente pelo serviço de ingestão a
-- partir da API pública OpenF1. Não há seed de dados de demonstração.

CREATE EXTENSION IF NOT EXISTS timescaledb CASCADE;

-- 1. Payloads brutos (armazém imutável de auditoria)
CREATE TABLE IF NOT EXISTS raw_payloads (
    id BIGSERIAL PRIMARY KEY,
    source VARCHAR(64) NOT NULL,
    endpoint VARCHAR(64) NOT NULL,
    session_key BIGINT NOT NULL,
    driver_number INTEGER,
    requested_url TEXT NOT NULL,
    fetched_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    schema_version VARCHAR(16) NOT NULL DEFAULT '1.0.0',
    payload_sha256 CHAR(64) NOT NULL,
    payload JSONB NOT NULL,
    UNIQUE (endpoint, session_key, driver_number, payload_sha256)
);
CREATE INDEX IF NOT EXISTS idx_raw_session_endpoint ON raw_payloads (session_key, endpoint, fetched_at DESC);

-- 2. Sessões
CREATE TABLE IF NOT EXISTS sessions (
    session_key BIGINT PRIMARY KEY,
    meeting_key BIGINT,
    session_name VARCHAR(128) NOT NULL,
    session_type VARCHAR(64) NOT NULL,
    circuit_key INTEGER NOT NULL,
    circuit_name VARCHAR(128) NOT NULL,
    country_name VARCHAR(64) NOT NULL,
    location VARCHAR(128),
    date_start TIMESTAMPTZ NOT NULL,
    date_end TIMESTAMPTZ,
    year INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_sessions_year ON sessions (year, date_start DESC);

-- 3. Pilotos
CREATE TABLE IF NOT EXISTS drivers (
    session_key BIGINT NOT NULL REFERENCES sessions(session_key) ON DELETE CASCADE,
    driver_number INTEGER NOT NULL,
    broadcast_name VARCHAR(64) NOT NULL,
    full_name VARCHAR(128) NOT NULL,
    name_acronym VARCHAR(8) NOT NULL,
    team_name VARCHAR(128) NOT NULL,
    team_colour VARCHAR(16) NOT NULL,
    PRIMARY KEY (session_key, driver_number)
);

-- 4. Stints (composto e idade de pneu reais, origem do contexto de degradação)
CREATE TABLE IF NOT EXISTS stints (
    session_key BIGINT NOT NULL REFERENCES sessions(session_key) ON DELETE CASCADE,
    driver_number INTEGER NOT NULL,
    stint_number INTEGER NOT NULL,
    lap_start INTEGER NOT NULL,
    lap_end INTEGER NOT NULL,
    compound VARCHAR(32) NOT NULL DEFAULT 'UNKNOWN',
    tyre_age_at_start INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY (session_key, driver_number, stint_number)
);

-- 5. Voltas
CREATE TABLE IF NOT EXISTS laps (
    session_key BIGINT NOT NULL REFERENCES sessions(session_key) ON DELETE CASCADE,
    driver_number INTEGER NOT NULL,
    lap_number INTEGER NOT NULL,
    date_start TIMESTAMPTZ,
    lap_time_s NUMERIC(9,3),
    is_valid BOOLEAN NOT NULL DEFAULT TRUE,
    lap_kind VARCHAR(32) NOT NULL DEFAULT 'FLYING',
    compound VARCHAR(32) NOT NULL DEFAULT 'UNKNOWN',
    stint_number INTEGER NOT NULL DEFAULT 0,
    tyre_age_laps INTEGER NOT NULL DEFAULT 0,
    sector_1_s NUMERIC(9,3),
    sector_2_s NUMERIC(9,3),
    sector_3_s NUMERIC(9,3),
    i1_speed_kmh NUMERIC(6,1),
    i2_speed_kmh NUMERIC(6,1),
    st_speed_kmh NUMERIC(6,1),
    coverage_pct NUMERIC(5,2) NOT NULL DEFAULT 0.0,
    PRIMARY KEY (session_key, driver_number, lap_number)
);
CREATE INDEX IF NOT EXISTS idx_laps_session_driver ON laps (session_key, driver_number, lap_number);

-- 6. Amostras de telemetria da ECU (hypertable)
CREATE TABLE IF NOT EXISTS telemetry_samples (
    occurred_at TIMESTAMPTZ NOT NULL,
    session_key BIGINT NOT NULL,
    driver_number INTEGER NOT NULL,
    lap_number INTEGER,
    speed_kmh NUMERIC(6,2) NOT NULL,
    throttle_pct NUMERIC(5,2) NOT NULL,
    brake_pct NUMERIC(5,2) NOT NULL,
    rpm INTEGER NOT NULL,
    gear SMALLINT NOT NULL,
    drs_state SMALLINT NOT NULL,
    PRIMARY KEY (session_key, driver_number, occurred_at)
);
SELECT create_hypertable('telemetry_samples', 'occurred_at', if_not_exists => TRUE);
CREATE INDEX IF NOT EXISTS idx_telemetry_lap ON telemetry_samples (session_key, driver_number, lap_number, occurred_at);

-- 7. Amostras de posição (traçado real do circuito)
CREATE TABLE IF NOT EXISTS location_samples (
    occurred_at TIMESTAMPTZ NOT NULL,
    session_key BIGINT NOT NULL,
    driver_number INTEGER NOT NULL,
    lap_number INTEGER,
    x DOUBLE PRECISION NOT NULL,
    y DOUBLE PRECISION NOT NULL,
    z DOUBLE PRECISION,
    PRIMARY KEY (session_key, driver_number, occurred_at)
);
SELECT create_hypertable('location_samples', 'occurred_at', if_not_exists => TRUE);
CREATE INDEX IF NOT EXISTS idx_location_lap ON location_samples (session_key, driver_number, lap_number, occurred_at);

-- 8. Direção de prova
CREATE TABLE IF NOT EXISTS race_control_events (
    session_key BIGINT NOT NULL REFERENCES sessions(session_key) ON DELETE CASCADE,
    occurred_at TIMESTAMPTZ NOT NULL,
    category VARCHAR(64) NOT NULL,
    flag VARCHAR(32),
    scope VARCHAR(32),
    message TEXT NOT NULL,
    sector SMALLINT,
    driver_number SMALLINT,
    lap_number SMALLINT,
    PRIMARY KEY (session_key, occurred_at, message)
);
CREATE INDEX IF NOT EXISTS idx_race_control_session ON race_control_events (session_key, occurred_at);

-- 9. Condições de pista (entrada do modelo térmico de degradação)
CREATE TABLE IF NOT EXISTS weather_samples (
    session_key BIGINT NOT NULL REFERENCES sessions(session_key) ON DELETE CASCADE,
    occurred_at TIMESTAMPTZ NOT NULL,
    air_temperature_c NUMERIC(5,2),
    track_temperature_c NUMERIC(5,2),
    humidity_pct NUMERIC(5,2),
    pressure_mbar NUMERIC(7,2),
    wind_speed_ms NUMERIC(5,2),
    wind_direction_deg SMALLINT,
    rainfall SMALLINT,
    PRIMARY KEY (session_key, occurred_at)
);
