#include "warehouse.hpp"

#include "../../common/time/iso8601.hpp"

#include <libpq-fe.h>

#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <memory>
#include <sstream>

namespace apex::ingest {
namespace {

using Connection = std::unique_ptr<PGconn, decltype(&PQfinish)>;
using Result = std::unique_ptr<PGresult, decltype(&PQclear)>;

std::string number(double value, int precision) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << value;
    return oss.str();
}

std::string optional_number(const std::optional<double>& value, int precision) {
    return value ? number(*value, precision) : std::string("\\N");
}

/** Localiza o stint real que contém a volta, escolhendo o mais recente em caso de sobreposição. */
const apex::openf1::Stint* stint_for(const std::vector<apex::openf1::Stint>& stints, int32_t driver,
                                     int32_t lap) {
    const apex::openf1::Stint* match = nullptr;
    for (const auto& stint : stints) {
        if (stint.driver_number != driver) continue;
        if (lap < stint.lap_start || lap > stint.lap_end) continue;
        if (!match || stint.stint_number > match->stint_number) match = &stint;
    }
    return match;
}

} // namespace

Warehouse::Warehouse() {
    if (const char* value = std::getenv("APEX_DATABASE_URL")) connection_string_ = value;
}

bool Warehouse::healthy() const {
    if (!configured()) return false;
    Connection connection(PQconnectdb(connection_string_.c_str()), &PQfinish);
    const bool ok = connection && PQstatus(connection.get()) == CONNECTION_OK;
    if (!ok) last_error_ = connection ? PQerrorMessage(connection.get()) : "PQconnectdb returned null";
    return ok;
}

bool Warehouse::run_in_transaction(const std::function<bool(void*)>& body) {
    if (!configured()) {
        last_error_ = "APEX_DATABASE_URL is not set";
        return false;
    }
    Connection connection(PQconnectdb(connection_string_.c_str()), &PQfinish);
    if (!connection || PQstatus(connection.get()) != CONNECTION_OK) {
        last_error_ = connection ? PQerrorMessage(connection.get()) : "PQconnectdb returned null";
        return false;
    }

    Result begin(PQexec(connection.get(), "BEGIN"), &PQclear);
    if (!begin || PQresultStatus(begin.get()) != PGRES_COMMAND_OK) {
        last_error_ = PQerrorMessage(connection.get());
        return false;
    }

    if (!body(connection.get())) {
        Result rollback(PQexec(connection.get(), "ROLLBACK"), &PQclear);
        (void)rollback;
        return false;
    }

    Result commit(PQexec(connection.get(), "COMMIT"), &PQclear);
    if (!commit || PQresultStatus(commit.get()) != PGRES_COMMAND_OK) {
        last_error_ = PQerrorMessage(connection.get());
        return false;
    }
    last_error_.clear();
    return true;
}

namespace {

bool exec_params(PGconn* connection, std::string& error, const char* sql,
                 const std::vector<std::string>& parameters) {
    std::vector<const char*> values;
    values.reserve(parameters.size());
    for (const auto& parameter : parameters) {
        values.push_back(parameter == "\\N" ? nullptr : parameter.c_str());
    }
    Result result(PQexecParams(connection, sql, static_cast<int>(values.size()), nullptr,
                               values.data(), nullptr, nullptr, 0),
                  &PQclear);
    if (!result || PQresultStatus(result.get()) != PGRES_COMMAND_OK) {
        error = result ? PQresultErrorMessage(result.get()) : "PQexecParams returned null";
        return false;
    }
    return true;
}

} // namespace

bool Warehouse::upsert_session(const apex::openf1::Session& session) {
    return run_in_transaction([&](void* raw_connection) {
        auto* connection = static_cast<PGconn*>(raw_connection);
        static constexpr auto sql =
            "INSERT INTO sessions(session_key,meeting_key,session_name,session_type,circuit_key,"
            "circuit_name,country_name,location,date_start,date_end,year) "
            "VALUES($1,$2,$3,$4,$5,$6,$7,$8,$9::timestamptz,$10::timestamptz,$11) "
            "ON CONFLICT(session_key) DO UPDATE SET meeting_key=EXCLUDED.meeting_key,"
            "session_name=EXCLUDED.session_name,session_type=EXCLUDED.session_type,"
            "circuit_key=EXCLUDED.circuit_key,circuit_name=EXCLUDED.circuit_name,"
            "country_name=EXCLUDED.country_name,location=EXCLUDED.location,"
            "date_start=EXCLUDED.date_start,date_end=EXCLUDED.date_end,year=EXCLUDED.year";
        return exec_params(connection, last_error_, sql,
                           {std::to_string(session.session_key), std::to_string(session.meeting_key),
                            session.session_name, session.session_type,
                            std::to_string(session.circuit_key), session.circuit_name,
                            session.country_name, session.location,
                            session.date_start.empty() ? "\\N" : session.date_start,
                            session.date_end.empty() ? "\\N" : session.date_end,
                            std::to_string(session.year)});
    });
}

bool Warehouse::upsert_drivers(int64_t session_key, const std::vector<apex::openf1::Driver>& drivers) {
    return run_in_transaction([&](void* raw_connection) {
        auto* connection = static_cast<PGconn*>(raw_connection);
        static constexpr auto sql =
            "INSERT INTO drivers(session_key,driver_number,broadcast_name,full_name,name_acronym,"
            "team_name,team_colour) VALUES($1,$2,$3,$4,$5,$6,$7) "
            "ON CONFLICT(session_key,driver_number) DO UPDATE SET "
            "broadcast_name=EXCLUDED.broadcast_name,full_name=EXCLUDED.full_name,"
            "name_acronym=EXCLUDED.name_acronym,team_name=EXCLUDED.team_name,"
            "team_colour=EXCLUDED.team_colour";
        for (const auto& driver : drivers) {
            if (!exec_params(connection, last_error_, sql,
                             {std::to_string(session_key), std::to_string(driver.driver_number),
                              driver.broadcast_name, driver.full_name, driver.name_acronym,
                              driver.team_name, driver.team_colour})) {
                return false;
            }
        }
        return true;
    });
}

bool Warehouse::upsert_stints(int64_t session_key, const std::vector<apex::openf1::Stint>& stints) {
    return run_in_transaction([&](void* raw_connection) {
        auto* connection = static_cast<PGconn*>(raw_connection);
        static constexpr auto sql =
            "INSERT INTO stints(session_key,driver_number,stint_number,lap_start,lap_end,compound,"
            "tyre_age_at_start) VALUES($1,$2,$3,$4,$5,$6,$7) "
            "ON CONFLICT(session_key,driver_number,stint_number) DO UPDATE SET "
            "lap_start=EXCLUDED.lap_start,lap_end=EXCLUDED.lap_end,compound=EXCLUDED.compound,"
            "tyre_age_at_start=EXCLUDED.tyre_age_at_start";
        for (const auto& stint : stints) {
            if (!exec_params(connection, last_error_, sql,
                             {std::to_string(session_key), std::to_string(stint.driver_number),
                              std::to_string(stint.stint_number), std::to_string(stint.lap_start),
                              std::to_string(stint.lap_end), stint.compound,
                              std::to_string(stint.tyre_age_at_start)})) {
                return false;
            }
        }
        return true;
    });
}

bool Warehouse::upsert_laps(int64_t session_key, const std::vector<apex::openf1::Lap>& laps,
                            const std::vector<apex::openf1::Stint>& stints) {
    return run_in_transaction([&](void* raw_connection) {
        auto* connection = static_cast<PGconn*>(raw_connection);
        static constexpr auto sql =
            "INSERT INTO laps(session_key,driver_number,lap_number,date_start,lap_time_s,is_valid,"
            "lap_kind,compound,stint_number,tyre_age_laps,sector_1_s,sector_2_s,sector_3_s,"
            "i1_speed_kmh,i2_speed_kmh,st_speed_kmh) "
            "VALUES($1,$2,$3,$4::timestamptz,$5::numeric,$6::boolean,$7,$8,$9,$10,$11::numeric,"
            "$12::numeric,$13::numeric,$14::numeric,$15::numeric,$16::numeric) "
            "ON CONFLICT(session_key,driver_number,lap_number) DO UPDATE SET "
            "date_start=EXCLUDED.date_start,lap_time_s=EXCLUDED.lap_time_s,"
            "is_valid=EXCLUDED.is_valid,lap_kind=EXCLUDED.lap_kind,compound=EXCLUDED.compound,"
            "stint_number=EXCLUDED.stint_number,tyre_age_laps=EXCLUDED.tyre_age_laps,"
            "sector_1_s=EXCLUDED.sector_1_s,sector_2_s=EXCLUDED.sector_2_s,"
            "sector_3_s=EXCLUDED.sector_3_s,i1_speed_kmh=EXCLUDED.i1_speed_kmh,"
            "i2_speed_kmh=EXCLUDED.i2_speed_kmh,st_speed_kmh=EXCLUDED.st_speed_kmh";

        for (const auto& lap : laps) {
            const auto* stint = stint_for(stints, lap.driver_number, lap.lap_number);
            const int32_t tyre_age =
                stint ? stint->tyre_age_at_start + (lap.lap_number - stint->lap_start) : 0;
            const bool timed = lap.lap_duration_s.has_value();
            const std::string kind = lap.is_pit_out_lap ? "OUT_LAP" : (timed ? "FLYING" : "INVALID");

            if (!exec_params(
                    connection, last_error_, sql,
                    {std::to_string(session_key), std::to_string(lap.driver_number),
                     std::to_string(lap.lap_number),
                     lap.date_start_us > 0 ? apex::common::format_iso8601_us(lap.date_start_us) : "\\N",
                     optional_number(lap.lap_duration_s, 3),
                     (timed && !lap.is_pit_out_lap) ? "true" : "false", kind,
                     stint ? stint->compound : "UNKNOWN",
                     std::to_string(stint ? stint->stint_number : 0), std::to_string(tyre_age),
                     optional_number(lap.sector_1_s, 3), optional_number(lap.sector_2_s, 3),
                     optional_number(lap.sector_3_s, 3), optional_number(lap.i1_speed_kmh, 1),
                     optional_number(lap.i2_speed_kmh, 1), optional_number(lap.st_speed_kmh, 1)})) {
                return false;
            }
        }
        return true;
    });
}

bool Warehouse::upsert_race_control(int64_t session_key,
                                    const std::vector<apex::openf1::RaceControlEvent>& events) {
    return run_in_transaction([&](void* raw_connection) {
        auto* connection = static_cast<PGconn*>(raw_connection);
        static constexpr auto sql =
            "INSERT INTO race_control_events(session_key,occurred_at,category,flag,scope,message,"
            "sector,driver_number,lap_number) "
            "VALUES($1,$2::timestamptz,$3,$4,$5,$6,$7::smallint,$8::smallint,$9::smallint) "
            "ON CONFLICT(session_key,occurred_at,message) DO UPDATE SET "
            "category=EXCLUDED.category,flag=EXCLUDED.flag,scope=EXCLUDED.scope,"
            "sector=EXCLUDED.sector,driver_number=EXCLUDED.driver_number,"
            "lap_number=EXCLUDED.lap_number";
        for (const auto& event : events) {
            if (event.date_us == 0 || event.message.empty()) continue;
            if (!exec_params(connection, last_error_, sql,
                             {std::to_string(session_key),
                              apex::common::format_iso8601_us(event.date_us), event.category,
                              event.flag.empty() ? "\\N" : event.flag,
                              event.scope.empty() ? "\\N" : event.scope, event.message,
                              event.sector ? std::to_string(*event.sector) : "\\N",
                              event.driver_number ? std::to_string(*event.driver_number) : "\\N",
                              event.lap_number ? std::to_string(*event.lap_number) : "\\N"})) {
                return false;
            }
        }
        return true;
    });
}

bool Warehouse::upsert_weather(int64_t session_key,
                               const std::vector<apex::openf1::WeatherSample>& samples) {
    return run_in_transaction([&](void* raw_connection) {
        auto* connection = static_cast<PGconn*>(raw_connection);
        static constexpr auto sql =
            "INSERT INTO weather_samples(session_key,occurred_at,air_temperature_c,"
            "track_temperature_c,humidity_pct,pressure_mbar,wind_speed_ms,wind_direction_deg,rainfall) "
            "VALUES($1,$2::timestamptz,$3::numeric,$4::numeric,$5::numeric,$6::numeric,$7::numeric,"
            "$8::smallint,$9::smallint) "
            "ON CONFLICT(session_key,occurred_at) DO UPDATE SET "
            "air_temperature_c=EXCLUDED.air_temperature_c,"
            "track_temperature_c=EXCLUDED.track_temperature_c,humidity_pct=EXCLUDED.humidity_pct,"
            "pressure_mbar=EXCLUDED.pressure_mbar,wind_speed_ms=EXCLUDED.wind_speed_ms,"
            "wind_direction_deg=EXCLUDED.wind_direction_deg,rainfall=EXCLUDED.rainfall";
        for (const auto& sample : samples) {
            if (sample.date_us == 0) continue;
            if (!exec_params(connection, last_error_, sql,
                             {std::to_string(session_key),
                              apex::common::format_iso8601_us(sample.date_us),
                              number(sample.air_temperature_c, 2),
                              number(sample.track_temperature_c, 2), number(sample.humidity_pct, 2),
                              number(sample.pressure_mbar, 2), number(sample.wind_speed_ms, 2),
                              std::to_string(sample.wind_direction_deg),
                              std::to_string(sample.rainfall)})) {
                return false;
            }
        }
        return true;
    });
}

bool Warehouse::copy_series(const std::string& target_table, const std::string& temp_ddl,
                            const std::string& columns, const std::string& rows) {
    if (rows.empty()) return true;
    return run_in_transaction([&](void* raw_connection) {
        auto* connection = static_cast<PGconn*>(raw_connection);

        Result temp(PQexec(connection, temp_ddl.c_str()), &PQclear);
        if (!temp || PQresultStatus(temp.get()) != PGRES_COMMAND_OK) {
            last_error_ = PQerrorMessage(connection);
            return false;
        }

        const std::string copy_sql = "COPY apex_stage(" + columns + ") FROM STDIN";
        Result copy(PQexec(connection, copy_sql.c_str()), &PQclear);
        if (!copy || PQresultStatus(copy.get()) != PGRES_COPY_IN) {
            last_error_ = PQerrorMessage(connection);
            return false;
        }
        if (PQputCopyData(connection, rows.data(), static_cast<int>(rows.size())) != 1 ||
            PQputCopyEnd(connection, nullptr) != 1) {
            last_error_ = PQerrorMessage(connection);
            return false;
        }
        Result copy_result(PQgetResult(connection), &PQclear);
        if (!copy_result || PQresultStatus(copy_result.get()) != PGRES_COMMAND_OK) {
            last_error_ = copy_result ? PQresultErrorMessage(copy_result.get()) : "COPY failed";
            return false;
        }

        const std::string promote = "INSERT INTO " + target_table + "(" + columns + ") SELECT " +
                                    columns + " FROM apex_stage ON CONFLICT DO NOTHING";
        Result promoted(PQexec(connection, promote.c_str()), &PQclear);
        if (!promoted || PQresultStatus(promoted.get()) != PGRES_COMMAND_OK) {
            last_error_ = PQerrorMessage(connection);
            return false;
        }
        return true;
    });
}

bool Warehouse::copy_telemetry(int64_t session_key, int32_t driver_number, int32_t lap_number,
                               const std::vector<apex::openf1::CarSample>& samples) {
    std::ostringstream rows;
    for (const auto& sample : samples) {
        rows << apex::common::format_iso8601_us(sample.date_us) << '\t' << session_key << '\t'
             << driver_number << '\t' << lap_number << '\t' << number(sample.speed_kmh, 2) << '\t'
             << number(sample.throttle_pct, 2) << '\t' << number(sample.brake_pct, 2) << '\t'
             << sample.rpm << '\t' << sample.gear << '\t' << sample.drs_raw << '\n';
    }
    return copy_series("telemetry_samples",
                       "CREATE TEMP TABLE apex_stage(occurred_at timestamptz, session_key bigint, "
                       "driver_number integer, lap_number integer, speed_kmh numeric(6,2), "
                       "throttle_pct numeric(5,2), brake_pct numeric(5,2), rpm integer, "
                       "gear smallint, drs_state smallint) ON COMMIT DROP",
                       "occurred_at,session_key,driver_number,lap_number,speed_kmh,throttle_pct,"
                       "brake_pct,rpm,gear,drs_state",
                       rows.str());
}

bool Warehouse::copy_location(int64_t session_key, int32_t driver_number, int32_t lap_number,
                              const std::vector<apex::openf1::LocationSample>& samples) {
    std::ostringstream rows;
    for (const auto& sample : samples) {
        rows << apex::common::format_iso8601_us(sample.date_us) << '\t' << session_key << '\t'
             << driver_number << '\t' << lap_number << '\t' << number(sample.x, 2) << '\t'
             << number(sample.y, 2) << '\t' << number(sample.z, 2) << '\n';
    }
    return copy_series("location_samples",
                       "CREATE TEMP TABLE apex_stage(occurred_at timestamptz, session_key bigint, "
                       "driver_number integer, lap_number integer, x double precision, "
                       "y double precision, z double precision) ON COMMIT DROP",
                       "occurred_at,session_key,driver_number,lap_number,x,y,z", rows.str());
}

bool Warehouse::record_raw_payload(const RawArchive::Entry& entry) {
    return run_in_transaction([&](void* raw_connection) {
        auto* connection = static_cast<PGconn*>(raw_connection);
        static constexpr auto sql =
            "INSERT INTO raw_payloads(source,endpoint,session_key,driver_number,requested_url,"
            "payload_sha256,payload) VALUES('openf1',$1,$2,$3,$4,$5,$6::jsonb) "
            "ON CONFLICT(endpoint,session_key,driver_number,payload_sha256) DO NOTHING";
        return exec_params(
            connection, last_error_, sql,
            {entry.endpoint, std::to_string(entry.session_key),
             entry.driver_number ? std::to_string(*entry.driver_number) : "\\N", entry.requested_url,
             entry.sha256, entry.payload});
    });
}

bool Warehouse::update_lap_coverage(int64_t session_key, int32_t driver_number, int32_t lap_number,
                                    double coverage_pct) {
    return run_in_transaction([&](void* raw_connection) {
        auto* connection = static_cast<PGconn*>(raw_connection);
        static constexpr auto sql =
            "UPDATE laps SET coverage_pct=$4::numeric WHERE session_key=$1 AND driver_number=$2 "
            "AND lap_number=$3";
        return exec_params(connection, last_error_, sql,
                           {std::to_string(session_key), std::to_string(driver_number),
                            std::to_string(lap_number), number(coverage_pct, 2)});
    });
}

} // namespace apex::ingest
