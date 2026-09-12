#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace apex::gateway {

/** Linha bruta de telemetria já materializada da hypertable TimescaleDB. */
struct TelemetryRow {
    int64_t occurred_at_us{0};
    double speed_kmh{0.0};
    double throttle_pct{0.0};
    double brake_pct{0.0};
    int32_t rpm{0};
    int32_t gear{0};
    int32_t drs_state{0};
};

struct LocationRow {
    int64_t occurred_at_us{0};
    double x{0.0};
    double y{0.0};
};

struct LapRow {
    int32_t driver_number{0};
    int32_t lap_number{0};
    int64_t date_start_us{0};
    double lap_time_s{0.0};
    bool is_valid{true};
    std::string lap_kind{"FLYING"};
    std::string compound{"UNKNOWN"};
    int32_t stint_number{0};
    int32_t tyre_age_laps{0};
    std::optional<double> sector_1_s;
    std::optional<double> sector_2_s;
    std::optional<double> sector_3_s;
    std::optional<double> i1_speed_kmh;
    std::optional<double> i2_speed_kmh;
    std::optional<double> st_speed_kmh;
    double coverage_pct{0.0};
};

struct DriverRow {
    int32_t driver_number{0};
    std::string name_acronym;
    std::string full_name;
    std::string team_name;
    std::string team_colour;
};

struct SessionRow {
    int64_t session_key{0};
    int32_t circuit_key{0};
    std::string circuit_name;
    std::string country_name;
    std::string session_name;
    std::string session_type;
    int32_t year{0};
};

/**
 * Acesso somente-leitura ao armazém PostgreSQL/TimescaleDB alimentado pelo
 * serviço de ingestão. Sem `APEX_DATABASE_URL` configurado, todo método devolve
 * `std::nullopt` e o chamador recorre ao upstream — nunca a dados fabricados.
 */
class DatabaseRepository {
public:
    DatabaseRepository();

    bool configured() const;
    bool healthy() const;
    const std::string& last_error() const { return last_error_; }

    // Passagem direta em JSON, já no formato do contrato público.
    std::optional<std::string> sessions_json(std::optional<int> year) const;
    std::optional<std::string> drivers_json(int64_t session_key) const;
    std::optional<std::string> laps_json(int64_t session_key,
                                         std::optional<int32_t> driver_number) const;
    std::optional<std::string> race_control_json(int64_t session_key) const;
    std::optional<std::string> stints_json(int64_t session_key,
                                           std::optional<int32_t> driver_number) const;
    std::optional<std::string> weather_json(int64_t session_key) const;

    // Acessos tipados usados pelo motor de análise.
    std::optional<SessionRow> session(int64_t session_key) const;
    std::vector<DriverRow> drivers(int64_t session_key) const;
    std::vector<LapRow> laps(int64_t session_key, std::optional<int32_t> driver_number) const;
    std::vector<TelemetryRow> telemetry(int64_t session_key, int32_t driver_number,
                                        int32_t lap_number) const;
    std::vector<LocationRow> location(int64_t session_key, int32_t driver_number,
                                      std::optional<int32_t> lap_number) const;
    std::optional<double> track_temperature(int64_t session_key) const;
    std::optional<double> air_temperature(int64_t session_key) const;

private:
    std::string connection_string_;
    mutable std::string last_error_;

    std::optional<std::string> query_scalar(const std::string& sql,
                                            const std::vector<std::string>& parameters = {}) const;
    std::optional<std::vector<std::vector<std::optional<std::string>>>> query_rows(
        const std::string& sql, const std::vector<std::string>& parameters = {}) const;
};

} // namespace apex::gateway
