#pragma once

#include "raw_archive.hpp"

#include "../../common/openf1/openf1_client.hpp"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace apex::ingest {

/**
 * Escrita transacional no armazém PostgreSQL/TimescaleDB.
 *
 * Toda carga é idempotente: reexecutar a ingestão da mesma sessão converge para o
 * mesmo estado. As séries de alta frequência entram por `COPY` numa tabela
 * temporária e são promovidas com `ON CONFLICT DO NOTHING`, o que evita tanto o
 * custo de milhares de INSERTs quanto a duplicação de amostras.
 */
class Warehouse {
public:
    Warehouse();

    bool configured() const { return !connection_string_.empty(); }
    bool healthy() const;
    const std::string& last_error() const { return last_error_; }

    bool upsert_session(const apex::openf1::Session& session);
    bool upsert_drivers(int64_t session_key, const std::vector<apex::openf1::Driver>& drivers);
    bool upsert_stints(int64_t session_key, const std::vector<apex::openf1::Stint>& stints);
    /** As voltas recebem composto e idade de pneu a partir dos stints reais. */
    bool upsert_laps(int64_t session_key, const std::vector<apex::openf1::Lap>& laps,
                     const std::vector<apex::openf1::Stint>& stints);
    bool upsert_race_control(int64_t session_key,
                             const std::vector<apex::openf1::RaceControlEvent>& events);
    bool upsert_weather(int64_t session_key, const std::vector<apex::openf1::WeatherSample>& samples);
    bool copy_telemetry(int64_t session_key, int32_t driver_number, int32_t lap_number,
                        const std::vector<apex::openf1::CarSample>& samples);
    bool copy_location(int64_t session_key, int32_t driver_number, int32_t lap_number,
                       const std::vector<apex::openf1::LocationSample>& samples);
    bool record_raw_payload(const RawArchive::Entry& entry);
    /** Grava a cobertura efetivamente medida da volta após a carga da telemetria. */
    bool update_lap_coverage(int64_t session_key, int32_t driver_number, int32_t lap_number,
                             double coverage_pct);

private:
    std::string connection_string_;
    mutable std::string last_error_;

    bool run_in_transaction(const std::function<bool(void*)>& body);
    bool copy_series(const std::string& target_table, const std::string& temp_ddl,
                     const std::string& columns, const std::string& rows);
};

} // namespace apex::ingest
