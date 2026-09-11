#include "postgres_storage.hpp"

#include <cstdlib>
#include <libpq-fe.h>
#include <memory>

namespace apex::ingest {
namespace {
using Connection = std::unique_ptr<PGconn, decltype(&PQfinish)>;
using Result = std::unique_ptr<PGresult, decltype(&PQclear)>;

bool command(PGconn* connection, const char* sql, const std::vector<std::string>& parameters = {}) {
    std::vector<const char*> values;
    values.reserve(parameters.size());
    for (const auto& parameter : parameters) values.push_back(parameter.c_str());
    Result result(PQexecParams(connection, sql, static_cast<int>(values.size()), nullptr,
                               values.data(), nullptr, nullptr, 0), &PQclear);
    return result && PQresultStatus(result.get()) == PGRES_COMMAND_OK;
}

Connection connect(const std::string& connection_string) {
    return Connection(PQconnectdb(connection_string.c_str()), &PQfinish);
}
}

PostgreSQLStorage::PostgreSQLStorage() {
    if (const char* value = std::getenv("APEX_DATABASE_URL")) connection_string_ = value;
}

bool PostgreSQLStorage::configured() const { return !connection_string_.empty(); }

bool PostgreSQLStorage::healthy() const {
    if (!configured()) return false;
    auto connection = connect(connection_string_);
    return connection && PQstatus(connection.get()) == CONNECTION_OK;
}

bool PostgreSQLStorage::upsert_sessions(const std::vector<SessionRecord>& sessions) const {
    if (!configured()) return false;
    auto connection = connect(connection_string_);
    if (!connection || PQstatus(connection.get()) != CONNECTION_OK || !command(connection.get(), "BEGIN")) return false;
    constexpr auto sql =
        "INSERT INTO sessions(session_key,session_name,session_type,circuit_key,circuit_name,country_name,date_start,year) "
        "VALUES($1,$2,$3,$4,$5,$6,$7,$8) ON CONFLICT(session_key) DO UPDATE SET "
        "session_name=EXCLUDED.session_name,session_type=EXCLUDED.session_type,circuit_key=EXCLUDED.circuit_key,"
        "circuit_name=EXCLUDED.circuit_name,country_name=EXCLUDED.country_name,date_start=EXCLUDED.date_start,year=EXCLUDED.year";
    for (const auto& session : sessions) {
        if (!command(connection.get(), sql, {std::to_string(session.session_key), session.session_name, session.session_type,
                std::to_string(session.circuit_key), session.circuit_name, session.country_name, session.date_start,
                std::to_string(session.year)})) {
            command(connection.get(), "ROLLBACK");
            return false;
        }
    }
    return command(connection.get(), "COMMIT");
}

bool PostgreSQLStorage::upsert_drivers(const std::vector<DriverRecord>& drivers) const {
    if (!configured()) return false;
    auto connection = connect(connection_string_);
    if (!connection || PQstatus(connection.get()) != CONNECTION_OK || !command(connection.get(), "BEGIN")) return false;
    constexpr auto sql =
        "INSERT INTO drivers(session_key,driver_number,broadcast_name,full_name,name_acronym,team_name,team_colour) "
        "VALUES($1,$2,$3,$4,$5,$6,$7) ON CONFLICT(session_key,driver_number) DO UPDATE SET "
        "broadcast_name=EXCLUDED.broadcast_name,full_name=EXCLUDED.full_name,name_acronym=EXCLUDED.name_acronym,"
        "team_name=EXCLUDED.team_name,team_colour=EXCLUDED.team_colour";
    for (const auto& driver : drivers) {
        if (!command(connection.get(), sql, {std::to_string(driver.session_key), std::to_string(driver.driver_number),
                driver.broadcast_name, driver.full_name, driver.name_acronym, driver.team_name, driver.team_colour})) {
            command(connection.get(), "ROLLBACK");
            return false;
        }
    }
    return command(connection.get(), "COMMIT");
}

} // namespace apex::ingest
