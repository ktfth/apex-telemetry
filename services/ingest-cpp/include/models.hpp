#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include <chrono>

namespace apex::ingest {

/**
 * Representação UTC estrita em microssegundos desde Unix Epoch.
 * Proibido uso de float para tempo absoluto ou de parede.
 */
using MicrosecondsUTC = int64_t;

inline MicrosecondsUTC now_utc_microseconds() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()
    ).count();
}

/**
 * Payload bruto imutável auditável.
 */
struct RawPayloadRecord {
    std::string source{"openf1"};
    std::string source_record_id{};
    int64_t session_key{0};
    std::optional<int32_t> driver_number{std::nullopt};
    MicrosecondsUTC occurred_at_us{0};
    MicrosecondsUTC ingested_at_us{0};
    std::string schema_version{"1.0.0"};
    std::string payload_raw{};
};

/**
 * Sessão normalizada de GP ou Qualificação.
 */
struct SessionRecord {
    int64_t session_key{0};
    std::string session_name{};
    std::string session_type{};
    int32_t circuit_key{0};
    std::string circuit_name{};
    std::string country_name{};
    std::string date_start{};
    int32_t year{0};
};

/**
 * Piloto participante na sessão.
 */
struct DriverRecord {
    int64_t session_key{0};
    int32_t driver_number{0};
    std::string broadcast_name{};
    std::string full_name{};
    std::string name_acronym{};
    std::string team_name{};
    std::string team_colour{};
};

/**
 * Volta oficial registrada.
 */
struct LapRecord {
    int64_t session_key{0};
    int32_t driver_number{0};
    int32_t lap_number{0};
    int64_t lap_duration_us{0};
    bool is_valid{true};
    std::string lap_kind{"FLYING"};
    std::string compound{"UNKNOWN"};
    int32_t stint_number{1};
    std::optional<int64_t> sector_1_us{std::nullopt};
    std::optional<int64_t> sector_2_us{std::nullopt};
    std::optional<int64_t> sector_3_us{std::nullopt};
    double coverage_pct{100.0};
};

/**
 * Amostra telemétrica de alta frequência do carro.
 */
struct TelemetrySampleRecord {
    MicrosecondsUTC occurred_at_us{0};
    int64_t session_key{0};
    int32_t driver_number{0};
    int32_t lap_number{0};
    double speed_kmh{0.0};
    double throttle_pct{0.0};
    double brake_pct{0.0};
    int32_t rpm{0};
    int32_t gear{0};
    bool drs{false};
    double distance_m{0.0};
};

/**
 * Evento oficial de direção de prova.
 */
struct RaceControlRecord {
    MicrosecondsUTC occurred_at_us{0};
    int64_t session_key{0};
    std::string category{};
    std::string flag{};
    std::string message{};
    std::optional<int32_t> sector{std::nullopt};
    std::optional<int32_t> driver_number{std::nullopt};
    std::optional<int32_t> lap_number{std::nullopt};
};

} // namespace apex::ingest
