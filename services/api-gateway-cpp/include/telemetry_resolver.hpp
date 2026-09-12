#pragma once

#include "database_repository.hpp"
#include "spatial_alignment.hpp"

#include "../../common/openf1/openf1_client.hpp"

#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace apex::gateway {

/** Resultado de uma resolução: payload JSON já normalizado mais a origem verificável. */
struct ResolvedPayload {
    std::string json;
    std::string source; // "postgresql" | "openf1-upstream" | "cache"
};

/** Telemetria real de uma volta, pronta para o motor de alinhamento espacial. */
struct LapTelemetry {
    apex::analytics::LapMetadata metadata;
    std::vector<apex::analytics::RawSample> samples;
    std::string source;
};

/** Ponto do traçado real, derivado do transponder de posição. */
struct TrackPoint {
    double distance_m{0.0};
    double x{0.0};
    double y{0.0};
};

struct ResolveError {
    int status{503};
    std::string code{"UPSTREAM_UNAVAILABLE"};
    std::string message;
    explicit operator bool() const { return !message.empty(); }
};

/**
 * Camada única de resolução de dados do gateway.
 *
 * Ordem de precedência, sempre com dados medidos:
 *   1. PostgreSQL/TimescaleDB, quando `APEX_DATABASE_URL` está configurado e a
 *      sessão já foi ingerida;
 *   2. API pública OpenF1, consultada ao vivo e memorizada em cache com TTL;
 *   3. erro explícito.
 *
 * Nenhum caminho sintetiza telemetria: a ausência de dados é reportada ao cliente.
 */
class TelemetryResolver {
public:
    TelemetryResolver(std::shared_ptr<DatabaseRepository> database,
                      std::shared_ptr<apex::openf1::Client> upstream,
                      std::chrono::seconds cache_ttl = std::chrono::seconds(300));

    std::optional<ResolvedPayload> sessions(std::optional<int> year, ResolveError& error);
    std::optional<ResolvedPayload> drivers(int64_t session_key, ResolveError& error);
    std::optional<ResolvedPayload> laps(int64_t session_key, std::optional<int32_t> driver_number,
                                        ResolveError& error);
    std::optional<ResolvedPayload> race_control(int64_t session_key, ResolveError& error);
    std::optional<ResolvedPayload> stints(int64_t session_key, std::optional<int32_t> driver_number,
                                          ResolveError& error);
    std::optional<ResolvedPayload> weather(int64_t session_key, ResolveError& error);

    /** Traçado real do circuito derivado das amostras de posição de um piloto. */
    std::optional<ResolvedPayload> circuit_geometry(int64_t session_key, ResolveError& error);

    std::optional<apex::analytics::SessionMetadata> session_metadata(int64_t session_key,
                                                                     ResolveError& error);

    /** Telemetria alinhada ao instante zero da volta pedida. */
    std::optional<LapTelemetry> lap_telemetry(int64_t session_key, int32_t driver_number,
                                              int32_t lap_number, ResolveError& error);

    /** Lista de voltas cronometradas, usada para escolher voltas padrão comparáveis. */
    std::vector<apex::openf1::Lap> timed_laps(int64_t session_key, int32_t driver_number);

    bool database_configured() const;
    bool database_healthy() const;
    size_t cache_entries() const;
    void clear_cache();

private:
    struct CacheEntry {
        std::string payload;
        std::string source;
        std::chrono::steady_clock::time_point stored_at;
    };

    /**
     * Cache tipado com TTL para os catálogos do upstream.
     *
     * Uma única comparação precisa de pilotos, voltas e stints das duas voltas.
     * Sem memorizar, o mesmo `/drivers` é pedido várias vezes na mesma requisição
     * e a OpenF1 passa a recusar por excesso de chamadas (HTTP 429). O cache
     * guarda registros já interpretados, não só o JSON de passagem.
     */
    template <typename Key, typename Value>
    struct TypedCache {
        std::map<Key, std::pair<Value, std::chrono::steady_clock::time_point>> entries;
        std::mutex mutex;

        std::optional<Value> get(const Key& key, std::chrono::seconds ttl) {
            std::lock_guard<std::mutex> lock(mutex);
            const auto it = entries.find(key);
            if (it == entries.end()) return std::nullopt;
            if (std::chrono::steady_clock::now() - it->second.second > ttl) return std::nullopt;
            return it->second.first;
        }

        void put(const Key& key, Value value) {
            std::lock_guard<std::mutex> lock(mutex);
            if (entries.size() > 64) entries.clear();
            entries[key] = {std::move(value), std::chrono::steady_clock::now()};
        }

        void clear() {
            std::lock_guard<std::mutex> lock(mutex);
            entries.clear();
        }

        size_t size() {
            std::lock_guard<std::mutex> lock(mutex);
            return entries.size();
        }
    };

    std::shared_ptr<DatabaseRepository> database_;
    std::shared_ptr<apex::openf1::Client> upstream_;
    std::chrono::seconds cache_ttl_;

    mutable std::mutex cache_mutex_;
    std::map<std::string, CacheEntry> cache_;

    TypedCache<int64_t, std::vector<apex::openf1::Driver>> drivers_cache_;
    TypedCache<int64_t, std::vector<apex::openf1::Stint>> stints_cache_;
    TypedCache<std::string, std::vector<apex::openf1::Lap>> laps_cache_;
    TypedCache<int64_t, apex::analytics::SessionMetadata> session_cache_;

    std::optional<CacheEntry> cache_get(const std::string& key) const;
    void cache_put(const std::string& key, std::string payload, std::string source);

    std::vector<apex::openf1::Driver> upstream_drivers(int64_t session_key);
    std::vector<apex::openf1::Stint> upstream_stints(int64_t session_key);
    std::vector<apex::openf1::Lap> upstream_laps(int64_t session_key,
                                                 std::optional<int32_t> driver_number);
};

} // namespace apex::gateway
