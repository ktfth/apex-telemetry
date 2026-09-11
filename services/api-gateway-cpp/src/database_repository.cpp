#include "database_repository.hpp"

#include <cstdlib>
#include <libpq-fe.h>
#include <memory>

namespace apex::gateway {
namespace {
using Connection = std::unique_ptr<PGconn, decltype(&PQfinish)>;
using Result = std::unique_ptr<PGresult, decltype(&PQclear)>;
}

DatabaseRepository::DatabaseRepository() {
    if (const char* value = std::getenv("APEX_DATABASE_URL")) connection_string_ = value;
}

bool DatabaseRepository::configured() const { return !connection_string_.empty(); }

bool DatabaseRepository::healthy() const {
    if (!configured()) return false;
    Connection connection(PQconnectdb(connection_string_.c_str()), &PQfinish);
    return connection && PQstatus(connection.get()) == CONNECTION_OK;
}

std::optional<std::string> DatabaseRepository::query_json(
    const std::string& sql,
    const std::vector<std::string>& parameters
) const {
    if (!configured()) return std::nullopt;
    Connection connection(PQconnectdb(connection_string_.c_str()), &PQfinish);
    if (!connection || PQstatus(connection.get()) != CONNECTION_OK) return std::nullopt;

    std::vector<const char*> values;
    values.reserve(parameters.size());
    for (const auto& parameter : parameters) values.push_back(parameter.c_str());
    Result result(PQexecParams(connection.get(), sql.c_str(), static_cast<int>(values.size()), nullptr,
                               values.data(), nullptr, nullptr, 0), &PQclear);
    if (!result || PQresultStatus(result.get()) != PGRES_TUPLES_OK || PQntuples(result.get()) != 1)
        return std::nullopt;
    return std::string(PQgetvalue(result.get(), 0, 0));
}

std::optional<std::string> DatabaseRepository::sessions(std::optional<int> year) const {
    const std::string columns = "session_key,session_name,session_type,circuit_key,circuit_name,country_name,date_start,year";
    if (year) return query_json(
        "SELECT COALESCE(json_agg(row_to_json(s) ORDER BY s.date_start DESC),'[]'::json)::text FROM "
        "(SELECT " + columns + " FROM sessions WHERE year=$1 ORDER BY date_start DESC) s",
        {std::to_string(*year)});
    return query_json(
        "SELECT COALESCE(json_agg(row_to_json(s) ORDER BY s.date_start DESC),'[]'::json)::text FROM "
        "(SELECT " + columns + " FROM sessions ORDER BY date_start DESC) s");
}

std::optional<std::string> DatabaseRepository::drivers(int64_t session_key) const {
    return query_json(
        "SELECT COALESCE(json_agg(row_to_json(d) ORDER BY d.driver_number),'[]'::json)::text FROM "
        "(SELECT driver_number,broadcast_name,full_name,name_acronym,team_name,team_colour "
        "FROM drivers WHERE session_key=$1 ORDER BY driver_number) d",
        {std::to_string(session_key)});
}

std::optional<std::string> DatabaseRepository::laps(int64_t session_key, std::optional<int32_t> driver_number) const {
    const std::string columns = "driver_number,lap_number,lap_time_s,is_valid,lap_kind,compound,stint_number,"
                                "sector_1_s,sector_2_s,sector_3_s,coverage_pct";
    if (driver_number) return query_json(
        "SELECT COALESCE(json_agg(row_to_json(l) ORDER BY l.lap_number),'[]'::json)::text FROM "
        "(SELECT " + columns + " FROM laps WHERE session_key=$1 AND driver_number=$2 ORDER BY lap_number) l",
        {std::to_string(session_key), std::to_string(*driver_number)});
    return query_json(
        "SELECT COALESCE(json_agg(row_to_json(l) ORDER BY l.driver_number,l.lap_number),'[]'::json)::text FROM "
        "(SELECT " + columns + " FROM laps WHERE session_key=$1 ORDER BY driver_number,lap_number) l",
        {std::to_string(session_key)});
}

std::optional<std::string> DatabaseRepository::race_control(int64_t session_key) const {
    return query_json(
        "SELECT COALESCE(json_agg(row_to_json(r) ORDER BY r.occurred_at),'[]'::json)::text FROM "
        "(SELECT occurred_at,category,flag,message,sector,driver_number,lap_number "
        "FROM race_control_events WHERE session_key=$1 ORDER BY occurred_at) r",
        {std::to_string(session_key)});
}

} // namespace apex::gateway
