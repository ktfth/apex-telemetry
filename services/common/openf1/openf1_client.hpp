#pragma once

#include "../http/http_client.hpp"
#include "../time/iso8601.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace apex::openf1 {

using apex::common::MicrosecondsUTC;

struct Session {
    int64_t session_key{0};
    int64_t meeting_key{0};
    std::string session_name;
    std::string session_type;
    int32_t circuit_key{0};
    std::string circuit_name;
    std::string country_name;
    std::string location;
    std::string date_start;
    std::string date_end;
    int32_t year{0};
};

struct Driver {
    int64_t session_key{0};
    int32_t driver_number{0};
    std::string broadcast_name;
    std::string full_name;
    std::string name_acronym;
    std::string team_name;
    std::string team_colour;
};

struct Lap {
    int64_t session_key{0};
    int32_t driver_number{0};
    int32_t lap_number{0};
    MicrosecondsUTC date_start_us{0};
    std::optional<double> lap_duration_s;
    std::optional<double> sector_1_s;
    std::optional<double> sector_2_s;
    std::optional<double> sector_3_s;
    std::optional<double> i1_speed_kmh;
    std::optional<double> i2_speed_kmh;
    std::optional<double> st_speed_kmh;
    bool is_pit_out_lap{false};
};

struct Stint {
    int64_t session_key{0};
    int32_t driver_number{0};
    int32_t stint_number{0};
    int32_t lap_start{0};
    int32_t lap_end{0};
    int32_t tyre_age_at_start{0};
    std::string compound{"UNKNOWN"};
};

struct CarSample {
    MicrosecondsUTC date_us{0};
    double speed_kmh{0.0};
    double throttle_pct{0.0};
    double brake_pct{0.0};
    int32_t rpm{0};
    int32_t gear{0};
    int32_t drs_raw{0};
    /** OpenF1 codifica o DRS em estados; 10/12/14 significam aba efetivamente aberta. */
    bool drs_active() const { return drs_raw == 10 || drs_raw == 12 || drs_raw == 14; }
};

struct LocationSample {
    MicrosecondsUTC date_us{0};
    double x{0.0};
    double y{0.0};
    double z{0.0};
};

struct RaceControlEvent {
    MicrosecondsUTC date_us{0};
    std::string category;
    std::string flag;
    std::string scope;
    std::string message;
    std::optional<int32_t> sector;
    std::optional<int32_t> driver_number;
    std::optional<int32_t> lap_number;
};

struct WeatherSample {
    MicrosecondsUTC date_us{0};
    double air_temperature_c{0.0};
    double track_temperature_c{0.0};
    double humidity_pct{0.0};
    double pressure_mbar{0.0};
    double wind_speed_ms{0.0};
    int32_t wind_direction_deg{0};
    int32_t rainfall{0};
};

/**
 * Cliente da API pública OpenF1 (https://openf1.org).
 * Todas as chamadas são HTTP reais; nenhuma resposta é sintetizada. Um erro de
 * upstream devolve coleção vazia e preenche `last_error()`, de modo que o
 * chamador possa propagar a falha em vez de mascará-la com dados fictícios.
 */
class Client {
public:
    explicit Client(std::shared_ptr<apex::common::HttpClient> http,
                    std::string base_url = "https://api.openf1.org/v1");

    std::vector<Session> sessions(std::optional<int> year = std::nullopt,
                                  const std::string& session_name = {},
                                  const std::string& country_name = {});
    std::optional<Session> session(int64_t session_key);
    std::vector<Driver> drivers(int64_t session_key);
    std::vector<Lap> laps(int64_t session_key, std::optional<int32_t> driver_number = std::nullopt);
    std::vector<Stint> stints(int64_t session_key, std::optional<int32_t> driver_number = std::nullopt);
    std::vector<CarSample> car_data(int64_t session_key, int32_t driver_number,
                                    MicrosecondsUTC from_us, MicrosecondsUTC to_us);
    std::vector<LocationSample> location(int64_t session_key, int32_t driver_number,
                                         MicrosecondsUTC from_us, MicrosecondsUTC to_us);
    std::vector<RaceControlEvent> race_control(int64_t session_key);
    std::vector<WeatherSample> weather(int64_t session_key);

    /**
     * Observador do corpo bruto de cada resposta bem-sucedida.
     * A ingestão usa isto para arquivar exatamente o que recebeu, sem depender de
     * uma reserialização dos registros já interpretados.
     */
    using PayloadObserver =
        std::function<void(std::string_view endpoint, const std::string& url, const std::string& body)>;
    void set_payload_observer(PayloadObserver observer) { observer_ = std::move(observer); }

    /**
     * Observador de toda requisição concluída, bem-sucedida ou não.
     * O gateway usa isto para contabilizar o tráfego real ao upstream nas métricas.
     */
    using RequestObserver = std::function<void(std::string_view endpoint, int status, bool success)>;
    void set_request_observer(RequestObserver observer) { request_observer_ = std::move(observer); }

    const std::string& last_error() const { return last_error_; }
    bool ok() const { return last_error_.empty(); }
    /**
     * Status HTTP da última chamada. Distingue "o upstream recusou" de "o upstream
     * respondeu que não existe": são erros diferentes para quem consome a API.
     */
    int last_status() const { return last_status_; }
    bool last_was_rate_limited() const { return last_status_ == 429; }
    const std::string& base_url() const { return base_url_; }

private:
    std::shared_ptr<apex::common::HttpClient> http_;
    std::string base_url_;
    std::string last_error_;
    int last_status_{0};
    PayloadObserver observer_;
    RequestObserver request_observer_;

    std::optional<std::string> fetch(std::string_view endpoint, const std::string& path_with_query);
};

} // namespace apex::openf1
