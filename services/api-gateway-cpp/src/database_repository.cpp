#include "database_repository.hpp"

#include "../../common/time/iso8601.hpp"

#include <libpq-fe.h>

#include <cstdlib>
#include <memory>

namespace apex::gateway {
namespace {

using Connection = std::unique_ptr<PGconn, decltype(&PQfinish)>;
using Result = std::unique_ptr<PGresult, decltype(&PQclear)>;

std::optional<double> to_double(const std::optional<std::string>& value) {
    if (!value || value->empty()) return std::nullopt;
    try {
        return std::stod(*value);
    } catch (...) {
        return std::nullopt;
    }
}

int64_t to_int(const std::optional<std::string>& value, int64_t fallback = 0) {
    if (!value || value->empty()) return fallback;
    try {
        return std::stoll(*value);
    } catch (...) {
        return fallback;
    }
}

std::string to_text(const std::optional<std::string>& value, std::string fallback = {}) {
    return value ? *value : fallback;
}

int64_t to_time_us(const std::optional<std::string>& value) {
    if (!value || value->empty()) return 0;
    return apex::common::parse_iso8601_us(*value).value_or(0);
}

} // namespace

DatabaseRepository::DatabaseRepository() {
    if (const char* value = std::getenv("APEX_DATABASE_URL")) connection_string_ = value;
}

bool DatabaseRepository::configured() const { return !connection_string_.empty(); }

bool DatabaseRepository::healthy() const {
    if (!configured()) return false;
    Connection connection(PQconnectdb(connection_string_.c_str()), &PQfinish);
    const bool ok = connection && PQstatus(connection.get()) == CONNECTION_OK;
    if (!ok && connection) last_error_ = PQerrorMessage(connection.get());
    return ok;
}

std::optional<std::vector<std::vector<std::optional<std::string>>>> DatabaseRepository::query_rows(
    const std::string& sql, const std::vector<std::string>& parameters) const {
    if (!configured()) return std::nullopt;
    Connection connection(PQconnectdb(connection_string_.c_str()), &PQfinish);
    if (!connection || PQstatus(connection.get()) != CONNECTION_OK) {
        last_error_ = connection ? PQerrorMessage(connection.get()) : "PQconnectdb returned null";
        return std::nullopt;
    }

    std::vector<const char*> values;
    values.reserve(parameters.size());
    for (const auto& parameter : parameters) values.push_back(parameter.c_str());

    Result result(PQexecParams(connection.get(), sql.c_str(), static_cast<int>(values.size()), nullptr,
                               values.data(), nullptr, nullptr, 0),
                  &PQclear);
    if (!result || PQresultStatus(result.get()) != PGRES_TUPLES_OK) {
        last_error_ = result ? PQresultErrorMessage(result.get()) : "PQexecParams returned null";
        return std::nullopt;
    }

    const int rows = PQntuples(result.get());
    const int columns = PQnfields(result.get());
    std::vector<std::vector<std::optional<std::string>>> table;
    table.reserve(static_cast<size_t>(rows));
    for (int row = 0; row < rows; ++row) {
        std::vector<std::optional<std::string>> record;
        record.reserve(static_cast<size_t>(columns));
        for (int column = 0; column < columns; ++column) {
            if (PQgetisnull(result.get(), row, column)) {
                record.emplace_back(std::nullopt);
            } else {
                record.emplace_back(std::string(PQgetvalue(result.get(), row, column)));
            }
        }
        table.push_back(std::move(record));
    }
    last_error_.clear();
    return table;
}

std::optional<std::string> DatabaseRepository::query_scalar(
    const std::string& sql, const std::vector<std::string>& parameters) const {
    const auto rows = query_rows(sql, parameters);
    if (!rows || rows->size() != 1 || rows->front().empty()) return std::nullopt;
    return rows->front().front();
}

std::optional<std::string> DatabaseRepository::sessions_json(std::optional<int> year) const {
    static constexpr auto columns =
        "session_key,session_name,session_type,circuit_key,circuit_name,country_name,"
        "to_char(date_start,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS date_start,year";
    if (year) {
        return query_scalar(
            std::string("SELECT COALESCE(json_agg(row_to_json(s)),'[]'::json)::text FROM (SELECT ") +
                columns + " FROM sessions WHERE year=$1 ORDER BY date_start DESC) s",
            {std::to_string(*year)});
    }
    return query_scalar(
        std::string("SELECT COALESCE(json_agg(row_to_json(s)),'[]'::json)::text FROM (SELECT ") +
        columns + " FROM sessions ORDER BY date_start DESC) s");
}

std::optional<std::string> DatabaseRepository::drivers_json(int64_t session_key) const {
    return query_scalar(
        "SELECT COALESCE(json_agg(row_to_json(d)),'[]'::json)::text FROM "
        "(SELECT driver_number,broadcast_name,full_name,name_acronym,team_name,team_colour "
        "FROM drivers WHERE session_key=$1 ORDER BY driver_number) d",
        {std::to_string(session_key)});
}

std::optional<std::string> DatabaseRepository::laps_json(
    int64_t session_key, std::optional<int32_t> driver_number) const {
    static constexpr auto columns =
        "driver_number,lap_number,lap_time_s::float8 AS lap_time_s,is_valid,lap_kind,compound,"
        "stint_number,tyre_age_laps,sector_1_s::float8 AS sector_1_s,sector_2_s::float8 AS sector_2_s,"
        "sector_3_s::float8 AS sector_3_s,i1_speed_kmh::float8 AS i1_speed_kmh,"
        "i2_speed_kmh::float8 AS i2_speed_kmh,st_speed_kmh::float8 AS st_speed_kmh,"
        "coverage_pct::float8 AS coverage_pct";
    if (driver_number) {
        return query_scalar(
            std::string("SELECT COALESCE(json_agg(row_to_json(l)),'[]'::json)::text FROM (SELECT ") +
                columns +
                " FROM laps WHERE session_key=$1 AND driver_number=$2 ORDER BY lap_number) l",
            {std::to_string(session_key), std::to_string(*driver_number)});
    }
    return query_scalar(
        std::string("SELECT COALESCE(json_agg(row_to_json(l)),'[]'::json)::text FROM (SELECT ") +
            columns + " FROM laps WHERE session_key=$1 ORDER BY driver_number,lap_number) l",
        {std::to_string(session_key)});
}

std::optional<std::string> DatabaseRepository::race_control_json(int64_t session_key) const {
    return query_scalar(
        "SELECT COALESCE(json_agg(row_to_json(r)),'[]'::json)::text FROM "
        "(SELECT to_char(occurred_at,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS occurred_at,"
        "category,flag,scope,message,sector,driver_number,lap_number "
        "FROM race_control_events WHERE session_key=$1 ORDER BY occurred_at) r",
        {std::to_string(session_key)});
}

std::optional<std::string> DatabaseRepository::stints_json(
    int64_t session_key, std::optional<int32_t> driver_number) const {
    if (driver_number) {
        return query_scalar(
            "SELECT COALESCE(json_agg(row_to_json(s)),'[]'::json)::text FROM "
            "(SELECT driver_number,stint_number,lap_start,lap_end,compound,tyre_age_at_start "
            "FROM stints WHERE session_key=$1 AND driver_number=$2 ORDER BY stint_number) s",
            {std::to_string(session_key), std::to_string(*driver_number)});
    }
    return query_scalar(
        "SELECT COALESCE(json_agg(row_to_json(s)),'[]'::json)::text FROM "
        "(SELECT driver_number,stint_number,lap_start,lap_end,compound,tyre_age_at_start "
        "FROM stints WHERE session_key=$1 ORDER BY driver_number,stint_number) s",
        {std::to_string(session_key)});
}

std::optional<std::string> DatabaseRepository::weather_json(int64_t session_key) const {
    return query_scalar(
        "SELECT COALESCE(json_agg(row_to_json(w)),'[]'::json)::text FROM "
        "(SELECT to_char(occurred_at,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS occurred_at,"
        "air_temperature_c::float8 AS air_temperature_c,"
        "track_temperature_c::float8 AS track_temperature_c,humidity_pct::float8 AS humidity_pct,"
        "pressure_mbar::float8 AS pressure_mbar,wind_speed_ms::float8 AS wind_speed_ms,"
        "wind_direction_deg,rainfall "
        "FROM weather_samples WHERE session_key=$1 ORDER BY occurred_at) w",
        {std::to_string(session_key)});
}

std::optional<SessionRow> DatabaseRepository::session(int64_t session_key) const {
    const auto rows = query_rows(
        "SELECT session_key,circuit_key,circuit_name,country_name,session_name,session_type,year "
        "FROM sessions WHERE session_key=$1",
        {std::to_string(session_key)});
    if (!rows || rows->empty()) return std::nullopt;
    const auto& record = rows->front();
    SessionRow row;
    row.session_key = to_int(record[0]);
    row.circuit_key = static_cast<int32_t>(to_int(record[1]));
    row.circuit_name = to_text(record[2]);
    row.country_name = to_text(record[3]);
    row.session_name = to_text(record[4]);
    row.session_type = to_text(record[5]);
    row.year = static_cast<int32_t>(to_int(record[6]));
    return row;
}

std::vector<DriverRow> DatabaseRepository::drivers(int64_t session_key) const {
    std::vector<DriverRow> drivers;
    const auto rows = query_rows(
        "SELECT driver_number,name_acronym,full_name,team_name,team_colour FROM drivers "
        "WHERE session_key=$1 ORDER BY driver_number",
        {std::to_string(session_key)});
    if (!rows) return drivers;
    drivers.reserve(rows->size());
    for (const auto& record : *rows) {
        DriverRow driver;
        driver.driver_number = static_cast<int32_t>(to_int(record[0]));
        driver.name_acronym = to_text(record[1]);
        driver.full_name = to_text(record[2]);
        driver.team_name = to_text(record[3]);
        driver.team_colour = to_text(record[4]);
        drivers.push_back(std::move(driver));
    }
    return drivers;
}

std::vector<LapRow> DatabaseRepository::laps(int64_t session_key,
                                             std::optional<int32_t> driver_number) const {
    static constexpr auto select =
        "SELECT driver_number,lap_number,"
        "to_char(date_start,'YYYY-MM-DD\"T\"HH24:MI:SS.USZ'),"
        "lap_time_s::float8,is_valid,lap_kind,compound,stint_number,tyre_age_laps,"
        "sector_1_s::float8,sector_2_s::float8,sector_3_s::float8,"
        "i1_speed_kmh::float8,i2_speed_kmh::float8,st_speed_kmh::float8,coverage_pct::float8 "
        "FROM laps WHERE session_key=$1";

    std::optional<std::vector<std::vector<std::optional<std::string>>>> rows;
    if (driver_number) {
        rows = query_rows(std::string(select) + " AND driver_number=$2 ORDER BY lap_number",
                          {std::to_string(session_key), std::to_string(*driver_number)});
    } else {
        rows = query_rows(std::string(select) + " ORDER BY driver_number,lap_number",
                          {std::to_string(session_key)});
    }

    std::vector<LapRow> laps;
    if (!rows) return laps;
    laps.reserve(rows->size());
    for (const auto& record : *rows) {
        LapRow lap;
        lap.driver_number = static_cast<int32_t>(to_int(record[0]));
        lap.lap_number = static_cast<int32_t>(to_int(record[1]));
        lap.date_start_us = to_time_us(record[2]);
        lap.lap_time_s = to_double(record[3]).value_or(0.0);
        lap.is_valid = to_text(record[4], "t") == "t";
        lap.lap_kind = to_text(record[5], "FLYING");
        lap.compound = to_text(record[6], "UNKNOWN");
        lap.stint_number = static_cast<int32_t>(to_int(record[7]));
        lap.tyre_age_laps = static_cast<int32_t>(to_int(record[8]));
        lap.sector_1_s = to_double(record[9]);
        lap.sector_2_s = to_double(record[10]);
        lap.sector_3_s = to_double(record[11]);
        lap.i1_speed_kmh = to_double(record[12]);
        lap.i2_speed_kmh = to_double(record[13]);
        lap.st_speed_kmh = to_double(record[14]);
        lap.coverage_pct = to_double(record[15]).value_or(0.0);
        laps.push_back(std::move(lap));
    }
    return laps;
}

std::vector<TelemetryRow> DatabaseRepository::telemetry(int64_t session_key, int32_t driver_number,
                                                        int32_t lap_number) const {
    std::vector<TelemetryRow> samples;
    const auto rows = query_rows(
        "SELECT to_char(occurred_at,'YYYY-MM-DD\"T\"HH24:MI:SS.USZ'),speed_kmh::float8,"
        "throttle_pct::float8,brake_pct::float8,rpm,gear,drs_state FROM telemetry_samples "
        "WHERE session_key=$1 AND driver_number=$2 AND lap_number=$3 ORDER BY occurred_at",
        {std::to_string(session_key), std::to_string(driver_number), std::to_string(lap_number)});
    if (!rows) return samples;
    samples.reserve(rows->size());
    for (const auto& record : *rows) {
        TelemetryRow sample;
        sample.occurred_at_us = to_time_us(record[0]);
        sample.speed_kmh = to_double(record[1]).value_or(0.0);
        sample.throttle_pct = to_double(record[2]).value_or(0.0);
        sample.brake_pct = to_double(record[3]).value_or(0.0);
        sample.rpm = static_cast<int32_t>(to_int(record[4]));
        sample.gear = static_cast<int32_t>(to_int(record[5]));
        sample.drs_state = static_cast<int32_t>(to_int(record[6]));
        samples.push_back(sample);
    }
    return samples;
}

std::vector<LocationRow> DatabaseRepository::location(int64_t session_key, int32_t driver_number,
                                                      std::optional<int32_t> lap_number) const {
    static constexpr auto select =
        "SELECT to_char(occurred_at,'YYYY-MM-DD\"T\"HH24:MI:SS.USZ'),x,y FROM location_samples "
        "WHERE session_key=$1 AND driver_number=$2";

    std::optional<std::vector<std::vector<std::optional<std::string>>>> rows;
    if (lap_number) {
        rows = query_rows(std::string(select) + " AND lap_number=$3 ORDER BY occurred_at",
                          {std::to_string(session_key), std::to_string(driver_number),
                           std::to_string(*lap_number)});
    } else {
        rows = query_rows(std::string(select) + " ORDER BY occurred_at",
                          {std::to_string(session_key), std::to_string(driver_number)});
    }

    std::vector<LocationRow> samples;
    if (!rows) return samples;
    samples.reserve(rows->size());
    for (const auto& record : *rows) {
        LocationRow sample;
        sample.occurred_at_us = to_time_us(record[0]);
        sample.x = to_double(record[1]).value_or(0.0);
        sample.y = to_double(record[2]).value_or(0.0);
        samples.push_back(sample);
    }
    return samples;
}

std::optional<double> DatabaseRepository::track_temperature(int64_t session_key) const {
    return to_double(query_scalar(
        "SELECT AVG(track_temperature_c)::text FROM weather_samples WHERE session_key=$1",
        {std::to_string(session_key)}));
}

std::optional<double> DatabaseRepository::air_temperature(int64_t session_key) const {
    return to_double(query_scalar(
        "SELECT AVG(air_temperature_c)::text FROM weather_samples WHERE session_key=$1",
        {std::to_string(session_key)}));
}

} // namespace apex::gateway
