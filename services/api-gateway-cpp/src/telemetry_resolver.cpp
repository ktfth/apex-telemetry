#include "telemetry_resolver.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <utility>

namespace apex::gateway {
namespace {

using apex::analytics::SpatialAlignmentEngine;

std::string number(double value, int precision) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << value;
    return oss.str();
}

void json_optional(std::ostringstream& oss, const char* key, const std::optional<double>& value,
                   int precision) {
    oss << "\"" << key << "\":";
    if (value) oss << number(*value, precision);
    else oss << "null";
}

std::string quote(const std::string& text) {
    return "\"" + SpatialAlignmentEngine::json_escape(text) + "\"";
}

/**
 * Cobertura real da volta: fração do tempo de volta efetivamente coberta por
 * amostras, descontando lacunas acima de três vezes o intervalo mediano.
 */
double compute_coverage_pct(const std::vector<apex::analytics::RawSample>& samples,
                            double lap_time_s) {
    if (samples.size() < 2 || lap_time_s <= 0.0) return 0.0;

    std::vector<double> intervals;
    intervals.reserve(samples.size() - 1);
    for (size_t i = 1; i < samples.size(); ++i) {
        intervals.push_back(samples[i].time_s - samples[i - 1].time_s);
    }
    auto sorted = intervals;
    std::nth_element(sorted.begin(), sorted.begin() + static_cast<long>(sorted.size() / 2),
                     sorted.end());
    const double median_dt = sorted[sorted.size() / 2];
    const double gap_threshold = std::max(0.5, median_dt * 3.0);

    double missing = samples.front().time_s;                   // atraso até a primeira amostra
    missing += std::max(0.0, lap_time_s - samples.back().time_s); // e após a última
    for (const double interval : intervals) {
        if (interval > gap_threshold) missing += interval - median_dt;
    }
    return std::clamp(100.0 * (1.0 - missing / lap_time_s), 0.0, 100.0);
}

/**
 * Traduz uma falha de upstream em erro de API.
 *
 * "A OpenF1 recusou a chamada" e "a OpenF1 respondeu que isso não existe" são
 * situações diferentes: a primeira é transitória e merece 502/503, a segunda é
 * definitiva e merece 404. Colapsar as duas em 404 faz o cliente desistir de um
 * dado que existe.
 */
ResolveError upstream_failure(const apex::openf1::Client& client, const std::string& not_found_code,
                              const std::string& not_found_message) {
    if (client.last_was_rate_limited()) {
        return {429, "UPSTREAM_RATE_LIMITED", client.last_error()};
    }
    // Um 404 do upstream é a própria OpenF1 dizendo que o recurso não existe.
    if (!client.ok() && client.last_status() != 404) {
        return {502, "UPSTREAM_ERROR", client.last_error()};
    }
    return {404, not_found_code,
            not_found_message.empty() ? client.last_error() : not_found_message};
}

/** Classifica a volta a partir dos fatos disponíveis, sem inferência especulativa. */
std::string classify_lap(bool is_pit_out, bool has_duration) {
    if (is_pit_out) return "OUT_LAP";
    if (!has_duration) return "INVALID";
    return "FLYING";
}

} // namespace

TelemetryResolver::TelemetryResolver(std::shared_ptr<DatabaseRepository> database,
                                     std::shared_ptr<apex::openf1::Client> upstream,
                                     std::chrono::seconds cache_ttl)
    : database_(std::move(database)), upstream_(std::move(upstream)), cache_ttl_(cache_ttl) {}

bool TelemetryResolver::database_configured() const {
    return database_ && database_->configured();
}

bool TelemetryResolver::database_healthy() const { return database_ && database_->healthy(); }

size_t TelemetryResolver::cache_entries() const {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto* mutable_self = const_cast<TelemetryResolver*>(this);
    return cache_.size() + mutable_self->drivers_cache_.size() + mutable_self->stints_cache_.size() +
           mutable_self->laps_cache_.size() + mutable_self->session_cache_.size();
}

std::optional<TelemetryResolver::CacheEntry> TelemetryResolver::cache_get(
    const std::string& key) const {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    const auto it = cache_.find(key);
    if (it == cache_.end()) return std::nullopt;
    if (std::chrono::steady_clock::now() - it->second.stored_at > cache_ttl_) return std::nullopt;
    return it->second;
}

void TelemetryResolver::cache_put(const std::string& key, std::string payload, std::string source) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    // Limite defensivo: o cache é um acelerador, não um armazém.
    if (cache_.size() > 256) cache_.clear();
    cache_[key] = CacheEntry{std::move(payload), std::move(source), std::chrono::steady_clock::now()};
}

std::vector<apex::openf1::Driver> TelemetryResolver::upstream_drivers(int64_t session_key) {
    if (!upstream_) return {};
    if (auto cached = drivers_cache_.get(session_key, cache_ttl_)) return *cached;
    auto drivers = upstream_->drivers(session_key);
    if (!drivers.empty()) drivers_cache_.put(session_key, drivers);
    return drivers;
}

std::vector<apex::openf1::Stint> TelemetryResolver::upstream_stints(int64_t session_key) {
    if (!upstream_) return {};
    if (auto cached = stints_cache_.get(session_key, cache_ttl_)) return *cached;
    auto stints = upstream_->stints(session_key);
    if (!stints.empty()) stints_cache_.put(session_key, stints);
    return stints;
}

std::vector<apex::openf1::Lap> TelemetryResolver::upstream_laps(
    int64_t session_key, std::optional<int32_t> driver_number) {
    if (!upstream_) return {};
    // As voltas da sessão inteira servem qualquer piloto: uma chamada abastece todos.
    const std::string key = std::to_string(session_key);
    if (auto cached = laps_cache_.get(key, cache_ttl_)) {
        if (!driver_number) return *cached;
        std::vector<apex::openf1::Lap> filtered;
        for (const auto& lap : *cached) {
            if (lap.driver_number == *driver_number) filtered.push_back(lap);
        }
        return filtered;
    }

    auto all = upstream_->laps(session_key, std::nullopt);
    if (!all.empty()) laps_cache_.put(key, all);
    if (!driver_number) return all;

    std::vector<apex::openf1::Lap> filtered;
    for (const auto& lap : all) {
        if (lap.driver_number == *driver_number) filtered.push_back(lap);
    }
    return filtered;
}

std::optional<ResolvedPayload> TelemetryResolver::sessions(std::optional<int> year,
                                                           ResolveError& error) {
    const std::string cache_key = "sessions:" + (year ? std::to_string(*year) : std::string("all"));
    if (const auto cached = cache_get(cache_key)) {
        return ResolvedPayload{cached->payload, cached->source};
    }

    if (database_configured()) {
        if (auto json = database_->sessions_json(year); json && *json != "[]") {
            cache_put(cache_key, *json, "postgresql");
            return ResolvedPayload{std::move(*json), "postgresql"};
        }
    }

    if (!upstream_) {
        error = {503, "NO_DATA_SOURCE", "Neither PostgreSQL nor the OpenF1 upstream is available"};
        return std::nullopt;
    }

    const auto sessions = upstream_->sessions(year);
    if (sessions.empty()) {
        error = upstream_failure(*upstream_, "SESSIONS_NOT_FOUND",
                                 "A OpenF1 não tem sessões para o ano solicitado");
        return std::nullopt;
    }

    std::ostringstream oss;
    oss << '[';
    for (size_t i = 0; i < sessions.size(); ++i) {
        const auto& session = sessions[i];
        if (i > 0) oss << ',';
        oss << "{\"session_key\":" << session.session_key << ",\"session_name\":"
            << quote(session.session_name) << ",\"session_type\":" << quote(session.session_type)
            << ",\"circuit_key\":" << session.circuit_key
            << ",\"circuit_name\":" << quote(session.circuit_name)
            << ",\"country_name\":" << quote(session.country_name)
            << ",\"date_start\":" << quote(session.date_start) << ",\"year\":" << session.year << '}';
    }
    oss << ']';

    cache_put(cache_key, oss.str(), "openf1-upstream");
    return ResolvedPayload{oss.str(), "openf1-upstream"};
}

std::optional<ResolvedPayload> TelemetryResolver::drivers(int64_t session_key, ResolveError& error) {
    const std::string cache_key = "drivers:" + std::to_string(session_key);
    if (const auto cached = cache_get(cache_key)) {
        return ResolvedPayload{cached->payload, cached->source};
    }

    if (database_configured()) {
        if (auto json = database_->drivers_json(session_key); json && *json != "[]") {
            cache_put(cache_key, *json, "postgresql");
            return ResolvedPayload{std::move(*json), "postgresql"};
        }
    }

    if (!upstream_) {
        error = {503, "NO_DATA_SOURCE", "Neither PostgreSQL nor the OpenF1 upstream is available"};
        return std::nullopt;
    }

    const auto drivers = upstream_drivers(session_key);
    if (drivers.empty()) {
        error = upstream_failure(*upstream_, "SESSION_NOT_FOUND",
                                 "A OpenF1 não tem lista de inscritos para esta sessão");
        return std::nullopt;
    }

    std::ostringstream oss;
    oss << '[';
    for (size_t i = 0; i < drivers.size(); ++i) {
        const auto& driver = drivers[i];
        if (i > 0) oss << ',';
        oss << "{\"driver_number\":" << driver.driver_number
            << ",\"broadcast_name\":" << quote(driver.broadcast_name)
            << ",\"full_name\":" << quote(driver.full_name)
            << ",\"name_acronym\":" << quote(driver.name_acronym)
            << ",\"team_name\":" << quote(driver.team_name)
            << ",\"team_colour\":" << quote(driver.team_colour) << '}';
    }
    oss << ']';

    cache_put(cache_key, oss.str(), "openf1-upstream");
    return ResolvedPayload{oss.str(), "openf1-upstream"};
}

std::optional<ResolvedPayload> TelemetryResolver::laps(int64_t session_key,
                                                       std::optional<int32_t> driver_number,
                                                       ResolveError& error) {
    const std::string cache_key = "laps:" + std::to_string(session_key) + ':' +
                                  (driver_number ? std::to_string(*driver_number) : "all");
    if (const auto cached = cache_get(cache_key)) {
        return ResolvedPayload{cached->payload, cached->source};
    }

    if (database_configured()) {
        if (auto json = database_->laps_json(session_key, driver_number); json && *json != "[]") {
            cache_put(cache_key, *json, "postgresql");
            return ResolvedPayload{std::move(*json), "postgresql"};
        }
    }

    if (!upstream_) {
        error = {503, "NO_DATA_SOURCE", "Neither PostgreSQL nor the OpenF1 upstream is available"};
        return std::nullopt;
    }

    const auto laps = upstream_laps(session_key, driver_number);
    if (laps.empty()) {
        error = upstream_failure(*upstream_, "LAPS_NOT_FOUND",
                                 "A OpenF1 não tem voltas para esta sessão/piloto");
        return std::nullopt;
    }

    // Composto e idade de pneu vêm dos stints reais, cruzados por faixa de voltas.
    const auto stints = upstream_stints(session_key);
    const auto stint_for = [&stints](int32_t driver, int32_t lap) -> const apex::openf1::Stint* {
        const apex::openf1::Stint* match = nullptr;
        for (const auto& stint : stints) {
            if (stint.driver_number != driver) continue;
            if (lap >= stint.lap_start && lap <= stint.lap_end) {
                if (!match || stint.stint_number > match->stint_number) match = &stint;
            }
        }
        return match;
    };

    std::ostringstream oss;
    oss << '[';
    bool first = true;
    for (const auto& lap : laps) {
        if (!first) oss << ',';
        first = false;
        const auto* stint = stint_for(lap.driver_number, lap.lap_number);
        const int32_t tyre_age =
            stint ? stint->tyre_age_at_start + (lap.lap_number - stint->lap_start) : 0;

        oss << "{\"driver_number\":" << lap.driver_number << ",\"lap_number\":" << lap.lap_number
            << ",\"lap_time_s\":"
            << (lap.lap_duration_s ? number(*lap.lap_duration_s, 3) : std::string("null"))
            << ",\"is_valid\":" << ((lap.lap_duration_s && !lap.is_pit_out_lap) ? "true" : "false")
            << ",\"lap_kind\":"
            << quote(classify_lap(lap.is_pit_out_lap, lap.lap_duration_s.has_value()))
            << ",\"compound\":" << quote(stint ? stint->compound : "UNKNOWN")
            << ",\"stint_number\":" << (stint ? stint->stint_number : 0)
            << ",\"tyre_age_laps\":" << tyre_age << ',';
        json_optional(oss, "sector_1_s", lap.sector_1_s, 3);
        oss << ',';
        json_optional(oss, "sector_2_s", lap.sector_2_s, 3);
        oss << ',';
        json_optional(oss, "sector_3_s", lap.sector_3_s, 3);
        oss << ',';
        json_optional(oss, "i1_speed_kmh", lap.i1_speed_kmh, 1);
        oss << ',';
        json_optional(oss, "i2_speed_kmh", lap.i2_speed_kmh, 1);
        oss << ',';
        json_optional(oss, "st_speed_kmh", lap.st_speed_kmh, 1);
        // A cobertura só é conhecida após buscar a telemetria da volta; aqui é declarada
        // como desconhecida em vez de assumida como 100%.
        oss << ",\"coverage_pct\":null}";
    }
    oss << ']';

    cache_put(cache_key, oss.str(), "openf1-upstream");
    return ResolvedPayload{oss.str(), "openf1-upstream"};
}

std::optional<ResolvedPayload> TelemetryResolver::race_control(int64_t session_key,
                                                               ResolveError& error) {
    const std::string cache_key = "race_control:" + std::to_string(session_key);
    if (const auto cached = cache_get(cache_key)) {
        return ResolvedPayload{cached->payload, cached->source};
    }

    if (database_configured()) {
        if (auto json = database_->race_control_json(session_key); json && *json != "[]") {
            cache_put(cache_key, *json, "postgresql");
            return ResolvedPayload{std::move(*json), "postgresql"};
        }
    }

    if (!upstream_) {
        error = {503, "NO_DATA_SOURCE", "Neither PostgreSQL nor the OpenF1 upstream is available"};
        return std::nullopt;
    }

    const auto events = upstream_->race_control(session_key);
    if (events.empty() && !upstream_->ok()) {
        error = upstream_failure(*upstream_, "RACE_CONTROL_NOT_FOUND", "");
        return std::nullopt;
    }

    std::ostringstream oss;
    oss << '[';
    for (size_t i = 0; i < events.size(); ++i) {
        const auto& event = events[i];
        if (i > 0) oss << ',';
        oss << "{\"occurred_at\":" << quote(apex::common::format_iso8601_us(event.date_us))
            << ",\"category\":" << quote(event.category) << ",\"flag\":" << quote(event.flag)
            << ",\"scope\":" << quote(event.scope) << ",\"message\":" << quote(event.message)
            << ",\"sector\":" << (event.sector ? std::to_string(*event.sector) : "null")
            << ",\"driver_number\":"
            << (event.driver_number ? std::to_string(*event.driver_number) : "null")
            << ",\"lap_number\":" << (event.lap_number ? std::to_string(*event.lap_number) : "null")
            << '}';
    }
    oss << ']';

    cache_put(cache_key, oss.str(), "openf1-upstream");
    return ResolvedPayload{oss.str(), "openf1-upstream"};
}

std::optional<ResolvedPayload> TelemetryResolver::stints(int64_t session_key,
                                                         std::optional<int32_t> driver_number,
                                                         ResolveError& error) {
    const std::string cache_key = "stints:" + std::to_string(session_key) + ':' +
                                  (driver_number ? std::to_string(*driver_number) : "all");
    if (const auto cached = cache_get(cache_key)) {
        return ResolvedPayload{cached->payload, cached->source};
    }

    if (database_configured()) {
        if (auto json = database_->stints_json(session_key, driver_number); json && *json != "[]") {
            cache_put(cache_key, *json, "postgresql");
            return ResolvedPayload{std::move(*json), "postgresql"};
        }
    }

    if (!upstream_) {
        error = {503, "NO_DATA_SOURCE", "Neither PostgreSQL nor the OpenF1 upstream is available"};
        return std::nullopt;
    }

    auto stints = upstream_->stints(session_key, driver_number);
    if (stints.empty() && !upstream_->ok()) {
        error = upstream_failure(*upstream_, "STINTS_NOT_FOUND", "");
        return std::nullopt;
    }

    std::ostringstream oss;
    oss << '[';
    for (size_t i = 0; i < stints.size(); ++i) {
        const auto& stint = stints[i];
        if (i > 0) oss << ',';
        oss << "{\"driver_number\":" << stint.driver_number
            << ",\"stint_number\":" << stint.stint_number << ",\"lap_start\":" << stint.lap_start
            << ",\"lap_end\":" << stint.lap_end << ",\"compound\":" << quote(stint.compound)
            << ",\"tyre_age_at_start\":" << stint.tyre_age_at_start << '}';
    }
    oss << ']';

    cache_put(cache_key, oss.str(), "openf1-upstream");
    return ResolvedPayload{oss.str(), "openf1-upstream"};
}

std::optional<ResolvedPayload> TelemetryResolver::weather(int64_t session_key, ResolveError& error) {
    const std::string cache_key = "weather:" + std::to_string(session_key);
    if (const auto cached = cache_get(cache_key)) {
        return ResolvedPayload{cached->payload, cached->source};
    }

    if (database_configured()) {
        if (auto json = database_->weather_json(session_key); json && *json != "[]") {
            cache_put(cache_key, *json, "postgresql");
            return ResolvedPayload{std::move(*json), "postgresql"};
        }
    }

    if (!upstream_) {
        error = {503, "NO_DATA_SOURCE", "Neither PostgreSQL nor the OpenF1 upstream is available"};
        return std::nullopt;
    }

    const auto samples = upstream_->weather(session_key);
    if (samples.empty() && !upstream_->ok()) {
        error = upstream_failure(*upstream_, "WEATHER_NOT_FOUND", "");
        return std::nullopt;
    }

    std::ostringstream oss;
    oss << '[';
    for (size_t i = 0; i < samples.size(); ++i) {
        const auto& sample = samples[i];
        if (i > 0) oss << ',';
        oss << "{\"occurred_at\":" << quote(apex::common::format_iso8601_us(sample.date_us))
            << ",\"air_temperature_c\":" << number(sample.air_temperature_c, 1)
            << ",\"track_temperature_c\":" << number(sample.track_temperature_c, 1)
            << ",\"humidity_pct\":" << number(sample.humidity_pct, 1)
            << ",\"pressure_mbar\":" << number(sample.pressure_mbar, 1)
            << ",\"wind_speed_ms\":" << number(sample.wind_speed_ms, 1)
            << ",\"wind_direction_deg\":" << sample.wind_direction_deg
            << ",\"rainfall\":" << sample.rainfall << '}';
    }
    oss << ']';

    cache_put(cache_key, oss.str(), "openf1-upstream");
    return ResolvedPayload{oss.str(), "openf1-upstream"};
}

std::optional<apex::analytics::SessionMetadata> TelemetryResolver::session_metadata(
    int64_t session_key, ResolveError& error) {
    if (auto cached = session_cache_.get(session_key, cache_ttl_)) return *cached;

    apex::analytics::SessionMetadata metadata;
    metadata.session_key = session_key;

    if (database_configured()) {
        if (const auto row = database_->session(session_key)) {
            metadata.circuit_key = row->circuit_key;
            metadata.circuit_name = row->circuit_name;
            metadata.country_name = row->country_name;
            metadata.session_name = row->session_name;
            metadata.track_temperature_c = database_->track_temperature(session_key);
            metadata.air_temperature_c = database_->air_temperature(session_key);
            metadata.data_source = "postgresql";
            session_cache_.put(session_key, metadata);
            return metadata;
        }
    }

    if (!upstream_) {
        error = {503, "NO_DATA_SOURCE", "Neither PostgreSQL nor the OpenF1 upstream is available"};
        return std::nullopt;
    }

    const auto session = upstream_->session(session_key);
    if (!session) {
        error = upstream_failure(*upstream_, "SESSION_NOT_FOUND",
                                 "A OpenF1 não tem sessão com esta chave");
        return std::nullopt;
    }

    metadata.circuit_key = session->circuit_key;
    metadata.circuit_name = session->circuit_name;
    metadata.country_name = session->country_name;
    metadata.session_name = session->session_name;
    metadata.data_source = "openf1-upstream";

    // Temperatura real da pista: média das leituras oficiais da sessão.
    const auto weather_samples = upstream_->weather(session_key);
    if (!weather_samples.empty()) {
        double track_sum = 0.0;
        double air_sum = 0.0;
        for (const auto& sample : weather_samples) {
            track_sum += sample.track_temperature_c;
            air_sum += sample.air_temperature_c;
        }
        const auto count = static_cast<double>(weather_samples.size());
        metadata.track_temperature_c = track_sum / count;
        metadata.air_temperature_c = air_sum / count;
    }

    session_cache_.put(session_key, metadata);
    return metadata;
}

std::vector<apex::openf1::Lap> TelemetryResolver::timed_laps(int64_t session_key,
                                                             int32_t driver_number) {
    std::vector<apex::openf1::Lap> laps;

    if (database_configured()) {
        for (const auto& row : database_->laps(session_key, driver_number)) {
            if (row.lap_time_s <= 0.0) continue;
            apex::openf1::Lap lap;
            lap.session_key = session_key;
            lap.driver_number = row.driver_number;
            lap.lap_number = row.lap_number;
            lap.date_start_us = row.date_start_us;
            lap.lap_duration_s = row.lap_time_s;
            lap.sector_1_s = row.sector_1_s;
            lap.sector_2_s = row.sector_2_s;
            lap.sector_3_s = row.sector_3_s;
            lap.i1_speed_kmh = row.i1_speed_kmh;
            lap.i2_speed_kmh = row.i2_speed_kmh;
            lap.st_speed_kmh = row.st_speed_kmh;
            lap.is_pit_out_lap = row.lap_kind == "OUT_LAP";
            laps.push_back(std::move(lap));
        }
        if (!laps.empty()) return laps;
    }

    for (auto& lap : upstream_laps(session_key, driver_number)) {
        if (!lap.lap_duration_s || lap.date_start_us == 0) continue;
        laps.push_back(std::move(lap));
    }
    return laps;
}

std::optional<LapTelemetry> TelemetryResolver::lap_telemetry(int64_t session_key,
                                                             int32_t driver_number,
                                                             int32_t lap_number,
                                                             ResolveError& error) {
    LapTelemetry telemetry;
    telemetry.metadata.driver_number = driver_number;
    telemetry.metadata.lap_number = lap_number;

    // 1. Identidade do piloto: sigla, equipe e cor reais.
    const auto apply_driver = [&](const std::string& code, const std::string& team,
                                  const std::string& colour) {
        telemetry.metadata.driver_code = code;
        telemetry.metadata.team_name = team;
        telemetry.metadata.team_colour = colour;
    };

    bool driver_found = false;
    if (database_configured()) {
        for (const auto& row : database_->drivers(session_key)) {
            if (row.driver_number != driver_number) continue;
            apply_driver(row.name_acronym, row.team_name, row.team_colour);
            driver_found = true;
            break;
        }
    }
    if (!driver_found) {
        for (const auto& driver : upstream_drivers(session_key)) {
            if (driver.driver_number != driver_number) continue;
            apply_driver(driver.name_acronym, driver.team_name, driver.team_colour);
            driver_found = true;
            break;
        }
    }
    if (!driver_found) {
        error = {404, "DRIVER_NOT_FOUND",
                 "Driver " + std::to_string(driver_number) + " did not take part in this session"};
        return std::nullopt;
    }

    // 2. A volta pedida, com tempos de setor e sensores de passagem reais.
    std::optional<apex::openf1::Lap> target;
    for (auto& lap : timed_laps(session_key, driver_number)) {
        if (lap.lap_number == lap_number) {
            target = std::move(lap);
            break;
        }
    }
    if (!target) {
        error = {404, "LAP_NOT_FOUND",
                 "Lap " + std::to_string(lap_number) + " of driver " + std::to_string(driver_number) +
                     " has no recorded lap time in this session"};
        return std::nullopt;
    }
    if (!target->lap_duration_s || target->date_start_us == 0) {
        error = {422, "LAP_NOT_TIMED",
                 "Lap " + std::to_string(lap_number) + " has no complete timing window"};
        return std::nullopt;
    }

    telemetry.metadata.lap_time_s = *target->lap_duration_s;
    telemetry.metadata.sector_1_s = target->sector_1_s;
    telemetry.metadata.sector_2_s = target->sector_2_s;
    telemetry.metadata.sector_3_s = target->sector_3_s;
    telemetry.metadata.i1_speed_kmh = target->i1_speed_kmh;
    telemetry.metadata.i2_speed_kmh = target->i2_speed_kmh;
    telemetry.metadata.st_speed_kmh = target->st_speed_kmh;

    // 3. Composto e idade de pneu a partir do stint real.
    const auto apply_stint = [&](int32_t stint_number, int32_t lap_start, int32_t age_at_start,
                                 const std::string& compound) {
        telemetry.metadata.stint_number = stint_number;
        telemetry.metadata.stint_lap = lap_number - lap_start + 1;
        telemetry.metadata.tyre_age_laps = age_at_start + (lap_number - lap_start);
        telemetry.metadata.compound = compound;
    };

    bool stint_found = false;
    if (database_configured()) {
        for (const auto& row : database_->laps(session_key, driver_number)) {
            if (row.lap_number != lap_number) continue;
            telemetry.metadata.compound = row.compound;
            telemetry.metadata.stint_number = row.stint_number;
            telemetry.metadata.tyre_age_laps = row.tyre_age_laps;
            stint_found = true;
            break;
        }
    }
    if (!stint_found) {
        for (const auto& stint : upstream_stints(session_key)) {
            if (stint.driver_number != driver_number) continue;
            if (lap_number < stint.lap_start || lap_number > stint.lap_end) continue;
            apply_stint(stint.stint_number, stint.lap_start, stint.tyre_age_at_start, stint.compound);
            stint_found = true;
        }
    }

    // 4. Telemetria da ECU dentro da janela real da volta.
    const auto from_us = target->date_start_us;
    const auto to_us = from_us + static_cast<int64_t>((*target->lap_duration_s + 0.4) * 1'000'000.0);

    if (database_configured()) {
        const auto rows = database_->telemetry(session_key, driver_number, lap_number);
        telemetry.samples.reserve(rows.size());
        for (const auto& row : rows) {
            apex::analytics::RawSample sample;
            sample.time_s = static_cast<double>(row.occurred_at_us - from_us) / 1'000'000.0;
            if (sample.time_s < -0.5) continue;
            sample.speed_kmh = row.speed_kmh;
            sample.throttle_pct = row.throttle_pct;
            sample.brake_pct = row.brake_pct;
            sample.rpm = row.rpm;
            sample.gear = row.gear;
            sample.drs = row.drs_state == 10 || row.drs_state == 12 || row.drs_state == 14;
            telemetry.samples.push_back(sample);
        }
        if (!telemetry.samples.empty()) telemetry.source = "postgresql";
    }

    if (telemetry.samples.empty()) {
        if (!upstream_) {
            error = {503, "NO_DATA_SOURCE", "No telemetry source is reachable"};
            return std::nullopt;
        }
        const auto car_data = upstream_->car_data(session_key, driver_number, from_us, to_us);
        telemetry.samples.reserve(car_data.size());
        for (const auto& sample : car_data) {
            apex::analytics::RawSample raw;
            raw.time_s = static_cast<double>(sample.date_us - from_us) / 1'000'000.0;
            raw.speed_kmh = sample.speed_kmh;
            raw.throttle_pct = sample.throttle_pct;
            raw.brake_pct = sample.brake_pct;
            raw.rpm = sample.rpm;
            raw.gear = sample.gear;
            raw.drs = sample.drs_active();
            telemetry.samples.push_back(raw);
        }
        telemetry.source = "openf1-upstream";
    }

    if (telemetry.samples.size() < 20) {
        error = {404, "TELEMETRY_UNAVAILABLE",
                 "Only " + std::to_string(telemetry.samples.size()) +
                     " ECU samples exist for this lap; a distance-aligned comparison needs at least 20"};
        return std::nullopt;
    }

    std::sort(telemetry.samples.begin(), telemetry.samples.end(),
              [](const auto& a, const auto& b) { return a.time_s < b.time_s; });

    telemetry.metadata.samples_count = static_cast<int32_t>(telemetry.samples.size());
    telemetry.metadata.coverage_pct =
        compute_coverage_pct(telemetry.samples, telemetry.metadata.lap_time_s);
    return telemetry;
}

std::optional<ResolvedPayload> TelemetryResolver::circuit_geometry(int64_t session_key,
                                                                   ResolveError& error) {
    const std::string cache_key = "circuit:" + std::to_string(session_key);
    if (const auto cached = cache_get(cache_key)) {
        return ResolvedPayload{cached->payload, cached->source};
    }

    const auto metadata = session_metadata(session_key, error);
    if (!metadata) return std::nullopt;

    // O traçado é reconstruído da volta mais rápida da sessão: é a passagem com a
    // trajetória mais limpa e a melhor densidade de amostras de posição.
    std::optional<apex::openf1::Lap> best;
    std::vector<int32_t> driver_numbers;

    if (database_configured()) {
        for (const auto& row : database_->drivers(session_key)) driver_numbers.push_back(row.driver_number);
    }
    if (driver_numbers.empty()) {
        for (const auto& driver : upstream_drivers(session_key)) driver_numbers.push_back(driver.driver_number);
    }
    if (driver_numbers.empty()) {
        error = {404, "SESSION_NOT_FOUND", "No driver entry list is available for this session"};
        return std::nullopt;
    }

    if (database_configured()) {
        for (const auto& row : database_->laps(session_key, std::nullopt)) {
            if (row.lap_time_s <= 0.0 || row.date_start_us == 0) continue;
            if (!best || row.lap_time_s < *best->lap_duration_s) {
                apex::openf1::Lap lap;
                lap.driver_number = row.driver_number;
                lap.lap_number = row.lap_number;
                lap.date_start_us = row.date_start_us;
                lap.lap_duration_s = row.lap_time_s;
                best = lap;
            }
        }
    }
    if (!best) {
        for (const auto& lap : upstream_laps(session_key, std::nullopt)) {
            if (!lap.lap_duration_s || lap.date_start_us == 0) continue;
            if (!best || *lap.lap_duration_s < *best->lap_duration_s) best = lap;
        }
    }
    if (!best) {
        error = {404, "LAPS_NOT_FOUND", "No timed lap is available to reconstruct the track layout"};
        return std::nullopt;
    }

    const auto from_us = best->date_start_us;
    const auto to_us = from_us + static_cast<int64_t>((*best->lap_duration_s + 0.4) * 1'000'000.0);

    std::vector<TrackPoint> track;
    std::string source = "openf1-upstream";

    // Telemetria da mesma volta fornece a distância percorrida por instante.
    ResolveError telemetry_error;
    const auto telemetry =
        lap_telemetry(session_key, best->driver_number, best->lap_number, telemetry_error);
    if (!telemetry) {
        error = telemetry_error;
        return std::nullopt;
    }
    const auto distance_samples = SpatialAlignmentEngine::integrate_distance(telemetry->samples);
    if (distance_samples.size() < 2) {
        error = {422, "TELEMETRY_UNAVAILABLE", "Distance integration produced no usable samples"};
        return std::nullopt;
    }

    const auto distance_at = [&distance_samples](double time_s) -> double {
        if (time_s <= distance_samples.front().time_s) return distance_samples.front().distance_m;
        for (size_t i = 1; i < distance_samples.size(); ++i) {
            if (distance_samples[i].time_s >= time_s) {
                const double t0 = distance_samples[i - 1].time_s;
                const double t1 = distance_samples[i].time_s;
                const double alpha = (t1 - t0) > 1e-9 ? (time_s - t0) / (t1 - t0) : 0.0;
                return distance_samples[i - 1].distance_m +
                       alpha * (distance_samples[i].distance_m - distance_samples[i - 1].distance_m);
            }
        }
        return distance_samples.back().distance_m;
    };

    std::vector<LocationRow> location_rows;
    if (database_configured()) {
        location_rows = database_->location(session_key, best->driver_number, best->lap_number);
        if (!location_rows.empty()) source = "postgresql";
    }
    if (location_rows.empty() && upstream_) {
        for (const auto& sample : upstream_->location(session_key, best->driver_number, from_us, to_us)) {
            location_rows.push_back({sample.date_us, sample.x, sample.y});
        }
    }
    if (location_rows.size() < 30) {
        error = {404, "TRACK_GEOMETRY_UNAVAILABLE",
                 "Only " + std::to_string(location_rows.size()) +
                     " transponder positions exist for the fastest lap of this session"};
        return std::nullopt;
    }

    track.reserve(location_rows.size());
    for (const auto& row : location_rows) {
        const double time_s = static_cast<double>(row.occurred_at_us - from_us) / 1'000'000.0;
        if (time_s < -0.2 || time_s > *best->lap_duration_s + 0.4) continue;
        track.push_back({distance_at(time_s), row.x, row.y});
    }
    std::sort(track.begin(), track.end(),
              [](const TrackPoint& a, const TrackPoint& b) { return a.distance_m < b.distance_m; });

    // Curvas reais, detectadas pelo canal de velocidade e posicionadas no traçado.
    const auto ref_grid = SpatialAlignmentEngine::resample_to_grid(distance_samples, 5.0, 25.0);
    const auto corners = SpatialAlignmentEngine::detect_corners(ref_grid);

    const auto position_at = [&track](double distance_m) -> std::pair<double, double> {
        if (track.empty()) return {0.0, 0.0};
        for (size_t i = 1; i < track.size(); ++i) {
            if (track[i].distance_m >= distance_m) {
                const double d0 = track[i - 1].distance_m;
                const double d1 = track[i].distance_m;
                const double alpha = (d1 - d0) > 1e-9 ? (distance_m - d0) / (d1 - d0) : 0.0;
                return {track[i - 1].x + alpha * (track[i].x - track[i - 1].x),
                        track[i - 1].y + alpha * (track[i].y - track[i - 1].y)};
            }
        }
        return {track.back().x, track.back().y};
    };

    std::ostringstream oss;
    oss << "{\"session_key\":" << session_key << ",\"circuit_key\":" << metadata->circuit_key
        << ",\"circuit_name\":" << quote(metadata->circuit_name)
        << ",\"country_name\":" << quote(metadata->country_name)
        << ",\"reference_driver_number\":" << best->driver_number
        << ",\"reference_lap_number\":" << best->lap_number
        << ",\"reference_lap_time_s\":" << number(*best->lap_duration_s, 3)
        << ",\"total_distance_m\":" << number(track.empty() ? 0.0 : track.back().distance_m, 1)
        << ",\"data_source\":" << quote(source) << ",\"path\":[";
    for (size_t i = 0; i < track.size(); ++i) {
        if (i > 0) oss << ',';
        oss << '[' << number(track[i].distance_m, 1) << ',' << number(track[i].x, 1) << ','
            << number(track[i].y, 1) << ']';
    }
    oss << "],\"corners\":[";
    for (size_t i = 0; i < corners.size(); ++i) {
        const auto& corner = corners[i];
        const auto [x, y] = position_at(corner.apex_distance_m);
        if (i > 0) oss << ',';
        oss << "{\"index\":" << corner.index << ",\"label\":" << quote(corner.label)
            << ",\"apex_distance_m\":" << number(corner.apex_distance_m, 1)
            << ",\"entry_distance_m\":" << number(corner.entry_distance_m, 1)
            << ",\"exit_distance_m\":" << number(corner.exit_distance_m, 1)
            << ",\"apex_speed_kmh\":" << number(corner.apex_speed_kmh, 1) << ",\"x\":" << number(x, 1)
            << ",\"y\":" << number(y, 1) << '}';
    }
    oss << "]}";

    cache_put(cache_key, oss.str(), source);
    return ResolvedPayload{oss.str(), source};
}

} // namespace apex::gateway
