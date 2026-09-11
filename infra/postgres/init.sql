-- ApexTelemetry Database Initialization Script
-- PostgreSQL 16 + TimescaleDB

CREATE EXTENSION IF NOT EXISTS timescaledb CASCADE;

-- 1. Raw Payloads (Immutable audit store)
CREATE TABLE IF NOT EXISTS raw_payloads (
    id BIGSERIAL PRIMARY KEY,
    source VARCHAR(64) NOT NULL,
    source_record_id VARCHAR(128),
    session_key INTEGER NOT NULL,
    driver_number INTEGER,
    occurred_at TIMESTAMPTZ NOT NULL,
    ingested_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    schema_version VARCHAR(16) NOT NULL DEFAULT '1.0.0',
    payload JSONB NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_raw_session_occurred ON raw_payloads (session_key, occurred_at);

-- 2. Sessions
CREATE TABLE IF NOT EXISTS sessions (
    session_key INTEGER PRIMARY KEY,
    session_name VARCHAR(128) NOT NULL,
    session_type VARCHAR(64) NOT NULL,
    circuit_key INTEGER NOT NULL,
    circuit_name VARCHAR(128) NOT NULL,
    country_name VARCHAR(64) NOT NULL,
    date_start TIMESTAMPTZ NOT NULL,
    year INTEGER NOT NULL
);

-- 3. Drivers
CREATE TABLE IF NOT EXISTS drivers (
    session_key INTEGER NOT NULL,
    driver_number INTEGER NOT NULL,
    broadcast_name VARCHAR(64) NOT NULL,
    full_name VARCHAR(128) NOT NULL,
    name_acronym VARCHAR(8) NOT NULL,
    team_name VARCHAR(128) NOT NULL,
    team_colour VARCHAR(16) NOT NULL,
    PRIMARY KEY (session_key, driver_number)
);

-- 4. Laps
CREATE TABLE IF NOT EXISTS laps (
    session_key INTEGER NOT NULL,
    driver_number INTEGER NOT NULL,
    lap_number INTEGER NOT NULL,
    lap_time_s NUMERIC(8,3) NOT NULL,
    is_valid BOOLEAN NOT NULL DEFAULT TRUE,
    lap_kind VARCHAR(32) NOT NULL DEFAULT 'FLYING',
    compound VARCHAR(32) NOT NULL DEFAULT 'UNKNOWN',
    stint_number INTEGER NOT NULL DEFAULT 1,
    sector_1_s NUMERIC(8,3),
    sector_2_s NUMERIC(8,3),
    sector_3_s NUMERIC(8,3),
    coverage_pct NUMERIC(5,2) NOT NULL DEFAULT 100.0,
    PRIMARY KEY (session_key, driver_number, lap_number)
);

-- 5. Telemetry Samples (TimescaleDB Hypertable)
CREATE TABLE IF NOT EXISTS telemetry_samples (
    occurred_at TIMESTAMPTZ NOT NULL,
    session_key INTEGER NOT NULL,
    driver_number INTEGER NOT NULL,
    lap_number INTEGER,
    speed_kmh NUMERIC(6,2) NOT NULL,
    throttle_pct NUMERIC(5,2) NOT NULL,
    brake_pct NUMERIC(5,2) NOT NULL,
    rpm INTEGER NOT NULL,
    gear SMALLINT NOT NULL,
    drs_state SMALLINT NOT NULL,
    distance_accum_m NUMERIC(9,2)
);
SELECT create_hypertable('telemetry_samples', 'occurred_at', if_not_exists => TRUE);
CREATE INDEX IF NOT EXISTS idx_telemetry_query ON telemetry_samples (session_key, driver_number, lap_number, occurred_at);

-- 6. Race Control Events
CREATE TABLE IF NOT EXISTS race_control_events (
    id BIGSERIAL PRIMARY KEY,
    session_key INTEGER NOT NULL,
    occurred_at TIMESTAMPTZ NOT NULL,
    category VARCHAR(64) NOT NULL,
    flag VARCHAR(32),
    message TEXT NOT NULL,
    sector SMALLINT,
    driver_number SMALLINT,
    lap_number SMALLINT
);
CREATE INDEX IF NOT EXISTS idx_race_control_session ON race_control_events (session_key, occurred_at);
