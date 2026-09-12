#include "openf1_client.hpp"

#include "../simdjson/simdjson.h"

#include <algorithm>
#include <utility>

namespace apex::openf1 {
namespace {

namespace sj = simdjson;

/** Lê um número (int ou double) tolerando ausência e `null`. */
std::optional<double> opt_number(sj::ondemand::object& obj, std::string_view key) {
    auto field = obj[key];
    if (field.error()) return std::nullopt;
    sj::ondemand::value value;
    if (field.get(value)) return std::nullopt;
    sj::ondemand::number number;
    if (value.get_number().get(number)) return std::nullopt;
    if (number.is_double()) return number.get_double();
    if (number.is_int64()) return static_cast<double>(number.get_int64());
    if (number.is_uint64()) return static_cast<double>(number.get_uint64());
    return std::nullopt;
}

std::optional<int64_t> opt_int(sj::ondemand::object& obj, std::string_view key) {
    const auto value = opt_number(obj, key);
    if (!value) return std::nullopt;
    return static_cast<int64_t>(*value);
}

std::string opt_string(sj::ondemand::object& obj, std::string_view key,
                       std::string fallback = {}) {
    auto field = obj[key];
    if (field.error()) return fallback;
    sj::ondemand::value value;
    if (field.get(value)) return fallback;
    std::string_view text;
    if (value.get_string().get(text)) return fallback;
    return std::string(text);
}

std::optional<bool> opt_bool(sj::ondemand::object& obj, std::string_view key) {
    auto field = obj[key];
    if (field.error()) return std::nullopt;
    sj::ondemand::value value;
    if (field.get(value)) return std::nullopt;
    bool flag{};
    if (value.get_bool().get(flag)) return std::nullopt;
    return flag;
}

std::optional<apex::common::MicrosecondsUTC> opt_time(sj::ondemand::object& obj,
                                                      std::string_view key) {
    const auto text = opt_string(obj, key);
    if (text.empty()) return std::nullopt;
    return apex::common::parse_iso8601_us(text);
}

/**
 * O canal de freio da OpenF1 é emitido como 0/100 em telemetria moderna e como
 * 0/1 em sessões antigas. Normalizamos para porcentagem sem inventar valores
 * intermediários que o sensor nunca produziu.
 */
double normalize_brake(double raw) {
    if (raw <= 0.0) return 0.0;
    if (raw <= 1.0) return 100.0;
    return std::min(100.0, raw);
}

std::string iso_query_value(apex::common::MicrosecondsUTC micros) {
    // A OpenF1 compara timestamps textualmente; segundos com 3 casas são aceitos.
    std::string iso = apex::common::format_iso8601_us(micros);
    if (!iso.empty() && iso.back() == 'Z') iso.pop_back();
    return iso;
}

} // namespace

Client::Client(std::shared_ptr<apex::common::HttpClient> http, std::string base_url)
    : http_(std::move(http)), base_url_(std::move(base_url)) {}

std::optional<std::string> Client::fetch(std::string_view endpoint,
                                        const std::string& path_with_query) {
    last_error_.clear();
    last_status_ = 0;
    if (!http_) {
        last_error_ = "HTTP client not configured";
        return std::nullopt;
    }
    const std::string url = base_url_ + path_with_query;
    const auto response = http_->get(url);
    last_status_ = response.status_code;
    if (request_observer_) request_observer_(endpoint, response.status_code, response.is_success());
    if (!response.is_success()) {
        last_error_ = response.is_rate_limited()
                          ? "a OpenF1 recusou por excesso de requisições (HTTP 429) em " +
                                path_with_query + "; tente novamente em alguns segundos"
                          : "openf1 " + path_with_query + ": " +
                                (response.error_message.empty() ? "unknown transport error"
                                                                : response.error_message);
        return std::nullopt;
    }
    if (observer_) observer_(endpoint, url, response.body);
    return response.body;
}

std::vector<Session> Client::sessions(std::optional<int> year, const std::string& session_name,
                                      const std::string& country_name) {
    std::string query = "/sessions?";
    if (year) query += "year=" + std::to_string(*year) + "&";
    if (!session_name.empty())
        query += "session_name=" + apex::common::HttpClient::url_encode(session_name) + "&";
    if (!country_name.empty())
        query += "country_name=" + apex::common::HttpClient::url_encode(country_name) + "&";
    if (query.back() == '&' || query.back() == '?') query.pop_back();

    const auto body = fetch("sessions", query);
    if (!body) return {};

    std::vector<Session> result;
    try {
        sj::ondemand::parser parser;
        sj::padded_string padded(*body);
        auto doc = parser.iterate(padded);
        for (auto item : doc.get_array()) {
            sj::ondemand::object obj = item.get_object();
            Session s;
            s.session_key = opt_int(obj, "session_key").value_or(0);
            s.meeting_key = opt_int(obj, "meeting_key").value_or(0);
            s.session_name = opt_string(obj, "session_name");
            s.session_type = opt_string(obj, "session_type");
            s.circuit_key = static_cast<int32_t>(opt_int(obj, "circuit_key").value_or(0));
            s.circuit_name = opt_string(obj, "circuit_short_name");
            s.country_name = opt_string(obj, "country_name");
            s.location = opt_string(obj, "location");
            s.date_start = opt_string(obj, "date_start");
            s.date_end = opt_string(obj, "date_end");
            s.year = static_cast<int32_t>(opt_int(obj, "year").value_or(0));
            if (s.session_key > 0) result.push_back(std::move(s));
        }
    } catch (const std::exception& e) {
        last_error_ = std::string("openf1 sessions parse: ") + e.what();
        return {};
    }
    return result;
}

std::optional<Session> Client::session(int64_t session_key) {
    const auto body = fetch("sessions", "/sessions?session_key=" + std::to_string(session_key));
    if (!body) return std::nullopt;
    try {
        sj::ondemand::parser parser;
        sj::padded_string padded(*body);
        auto doc = parser.iterate(padded);
        for (auto item : doc.get_array()) {
            sj::ondemand::object obj = item.get_object();
            Session s;
            s.session_key = opt_int(obj, "session_key").value_or(0);
            s.meeting_key = opt_int(obj, "meeting_key").value_or(0);
            s.session_name = opt_string(obj, "session_name");
            s.session_type = opt_string(obj, "session_type");
            s.circuit_key = static_cast<int32_t>(opt_int(obj, "circuit_key").value_or(0));
            s.circuit_name = opt_string(obj, "circuit_short_name");
            s.country_name = opt_string(obj, "country_name");
            s.location = opt_string(obj, "location");
            s.date_start = opt_string(obj, "date_start");
            s.date_end = opt_string(obj, "date_end");
            s.year = static_cast<int32_t>(opt_int(obj, "year").value_or(0));
            if (s.session_key > 0) return s;
        }
    } catch (const std::exception& e) {
        last_error_ = std::string("openf1 session parse: ") + e.what();
    }
    return std::nullopt;
}

std::vector<Driver> Client::drivers(int64_t session_key) {
    const auto body = fetch("drivers", "/drivers?session_key=" + std::to_string(session_key));
    if (!body) return {};

    std::vector<Driver> result;
    try {
        sj::ondemand::parser parser;
        sj::padded_string padded(*body);
        auto doc = parser.iterate(padded);
        for (auto item : doc.get_array()) {
            sj::ondemand::object obj = item.get_object();
            Driver d;
            d.session_key = session_key;
            d.driver_number = static_cast<int32_t>(opt_int(obj, "driver_number").value_or(0));
            d.broadcast_name = opt_string(obj, "broadcast_name");
            d.full_name = opt_string(obj, "full_name");
            d.name_acronym = opt_string(obj, "name_acronym");
            d.team_name = opt_string(obj, "team_name");
            const auto colour = opt_string(obj, "team_colour");
            d.team_colour = colour.empty() ? "#8b93a7"
                                           : (colour.front() == '#' ? colour : "#" + colour);
            if (d.driver_number > 0) result.push_back(std::move(d));
        }
    } catch (const std::exception& e) {
        last_error_ = std::string("openf1 drivers parse: ") + e.what();
        return {};
    }

    // A OpenF1 pode repetir um piloto por atualização de metadados; a última vence.
    std::stable_sort(result.begin(), result.end(),
                     [](const Driver& a, const Driver& b) { return a.driver_number < b.driver_number; });
    result.erase(std::unique(result.begin(), result.end(),
                             [](const Driver& a, const Driver& b) {
                                 return a.driver_number == b.driver_number;
                             }),
                 result.end());
    return result;
}

std::vector<Lap> Client::laps(int64_t session_key, std::optional<int32_t> driver_number) {
    std::string query = "/laps?session_key=" + std::to_string(session_key);
    if (driver_number) query += "&driver_number=" + std::to_string(*driver_number);

    const auto body = fetch("laps", query);
    if (!body) return {};

    std::vector<Lap> result;
    try {
        sj::ondemand::parser parser;
        sj::padded_string padded(*body);
        auto doc = parser.iterate(padded);
        for (auto item : doc.get_array()) {
            sj::ondemand::object obj = item.get_object();
            Lap l;
            l.session_key = session_key;
            l.driver_number = static_cast<int32_t>(opt_int(obj, "driver_number").value_or(0));
            l.lap_number = static_cast<int32_t>(opt_int(obj, "lap_number").value_or(0));
            l.date_start_us = opt_time(obj, "date_start").value_or(0);
            l.sector_1_s = opt_number(obj, "duration_sector_1");
            l.sector_2_s = opt_number(obj, "duration_sector_2");
            l.sector_3_s = opt_number(obj, "duration_sector_3");
            l.i1_speed_kmh = opt_number(obj, "i1_speed");
            l.i2_speed_kmh = opt_number(obj, "i2_speed");
            l.is_pit_out_lap = opt_bool(obj, "is_pit_out_lap").value_or(false);
            l.lap_duration_s = opt_number(obj, "lap_duration");
            l.st_speed_kmh = opt_number(obj, "st_speed");
            if (l.lap_number > 0) result.push_back(std::move(l));
        }
    } catch (const std::exception& e) {
        last_error_ = std::string("openf1 laps parse: ") + e.what();
        return {};
    }
    return result;
}

std::vector<Stint> Client::stints(int64_t session_key, std::optional<int32_t> driver_number) {
    std::string query = "/stints?session_key=" + std::to_string(session_key);
    if (driver_number) query += "&driver_number=" + std::to_string(*driver_number);

    const auto body = fetch("stints", query);
    if (!body) return {};

    std::vector<Stint> result;
    try {
        sj::ondemand::parser parser;
        sj::padded_string padded(*body);
        auto doc = parser.iterate(padded);
        for (auto item : doc.get_array()) {
            sj::ondemand::object obj = item.get_object();
            Stint s;
            s.session_key = session_key;
            s.driver_number = static_cast<int32_t>(opt_int(obj, "driver_number").value_or(0));
            s.stint_number = static_cast<int32_t>(opt_int(obj, "stint_number").value_or(0));
            s.lap_start = static_cast<int32_t>(opt_int(obj, "lap_start").value_or(0));
            s.lap_end = static_cast<int32_t>(opt_int(obj, "lap_end").value_or(0));
            s.compound = opt_string(obj, "compound", "UNKNOWN");
            s.tyre_age_at_start = static_cast<int32_t>(opt_int(obj, "tyre_age_at_start").value_or(0));
            if (s.compound.empty()) s.compound = "UNKNOWN";
            if (s.driver_number > 0) result.push_back(std::move(s));
        }
    } catch (const std::exception& e) {
        last_error_ = std::string("openf1 stints parse: ") + e.what();
        return {};
    }
    return result;
}

std::vector<CarSample> Client::car_data(int64_t session_key, int32_t driver_number,
                                        MicrosecondsUTC from_us, MicrosecondsUTC to_us) {
    const std::string query = "/car_data?session_key=" + std::to_string(session_key) +
                              "&driver_number=" + std::to_string(driver_number) +
                              "&date%3E=" + apex::common::HttpClient::url_encode(iso_query_value(from_us)) +
                              "&date%3C=" + apex::common::HttpClient::url_encode(iso_query_value(to_us));

    const auto body = fetch("car_data", query);
    if (!body) return {};

    std::vector<CarSample> result;
    try {
        sj::ondemand::parser parser;
        sj::padded_string padded(*body);
        auto doc = parser.iterate(padded);
        for (auto item : doc.get_array()) {
            sj::ondemand::object obj = item.get_object();
            CarSample s;
            const auto date = opt_time(obj, "date");
            if (!date) continue;
            s.date_us = *date;
            s.throttle_pct = opt_number(obj, "throttle").value_or(0.0);
            s.rpm = static_cast<int32_t>(opt_int(obj, "rpm").value_or(0));
            s.brake_pct = normalize_brake(opt_number(obj, "brake").value_or(0.0));
            s.speed_kmh = opt_number(obj, "speed").value_or(0.0);
            s.gear = static_cast<int32_t>(opt_int(obj, "n_gear").value_or(0));
            s.drs_raw = static_cast<int32_t>(opt_int(obj, "drs").value_or(0));
            result.push_back(s);
        }
    } catch (const std::exception& e) {
        last_error_ = std::string("openf1 car_data parse: ") + e.what();
        return {};
    }

    std::sort(result.begin(), result.end(),
              [](const CarSample& a, const CarSample& b) { return a.date_us < b.date_us; });
    return result;
}

std::vector<LocationSample> Client::location(int64_t session_key, int32_t driver_number,
                                             MicrosecondsUTC from_us, MicrosecondsUTC to_us) {
    const std::string query = "/location?session_key=" + std::to_string(session_key) +
                              "&driver_number=" + std::to_string(driver_number) +
                              "&date%3E=" + apex::common::HttpClient::url_encode(iso_query_value(from_us)) +
                              "&date%3C=" + apex::common::HttpClient::url_encode(iso_query_value(to_us));

    const auto body = fetch("location", query);
    if (!body) return {};

    std::vector<LocationSample> result;
    try {
        sj::ondemand::parser parser;
        sj::padded_string padded(*body);
        auto doc = parser.iterate(padded);
        for (auto item : doc.get_array()) {
            sj::ondemand::object obj = item.get_object();
            LocationSample s;
            const auto date = opt_time(obj, "date");
            if (!date) continue;
            s.date_us = *date;
            s.y = opt_number(obj, "y").value_or(0.0);
            s.x = opt_number(obj, "x").value_or(0.0);
            s.z = opt_number(obj, "z").value_or(0.0);
            // A OpenF1 emite (0,0) quando o transponder perde sinal: descartamos.
            if (s.x == 0.0 && s.y == 0.0) continue;
            result.push_back(s);
        }
    } catch (const std::exception& e) {
        last_error_ = std::string("openf1 location parse: ") + e.what();
        return {};
    }

    std::sort(result.begin(), result.end(),
              [](const LocationSample& a, const LocationSample& b) { return a.date_us < b.date_us; });
    return result;
}

std::vector<RaceControlEvent> Client::race_control(int64_t session_key) {
    const auto body = fetch("race_control", "/race_control?session_key=" + std::to_string(session_key));
    if (!body) return {};

    std::vector<RaceControlEvent> result;
    try {
        sj::ondemand::parser parser;
        sj::padded_string padded(*body);
        auto doc = parser.iterate(padded);
        for (auto item : doc.get_array()) {
            sj::ondemand::object obj = item.get_object();
            RaceControlEvent e;
            e.date_us = opt_time(obj, "date").value_or(0);
            e.driver_number = [&]() -> std::optional<int32_t> {
                const auto value = opt_int(obj, "driver_number");
                if (!value) return std::nullopt;
                return static_cast<int32_t>(*value);
            }();
            e.lap_number = [&]() -> std::optional<int32_t> {
                const auto value = opt_int(obj, "lap_number");
                if (!value) return std::nullopt;
                return static_cast<int32_t>(*value);
            }();
            e.category = opt_string(obj, "category");
            e.flag = opt_string(obj, "flag");
            e.scope = opt_string(obj, "scope");
            e.sector = [&]() -> std::optional<int32_t> {
                const auto value = opt_int(obj, "sector");
                if (!value) return std::nullopt;
                return static_cast<int32_t>(*value);
            }();
            e.message = opt_string(obj, "message");
            if (!e.message.empty()) result.push_back(std::move(e));
        }
    } catch (const std::exception& e) {
        last_error_ = std::string("openf1 race_control parse: ") + e.what();
        return {};
    }

    std::sort(result.begin(), result.end(), [](const RaceControlEvent& a, const RaceControlEvent& b) {
        return a.date_us < b.date_us;
    });
    return result;
}

std::vector<WeatherSample> Client::weather(int64_t session_key) {
    const auto body = fetch("weather", "/weather?session_key=" + std::to_string(session_key));
    if (!body) return {};

    std::vector<WeatherSample> result;
    try {
        sj::ondemand::parser parser;
        sj::padded_string padded(*body);
        auto doc = parser.iterate(padded);
        for (auto item : doc.get_array()) {
            sj::ondemand::object obj = item.get_object();
            WeatherSample w;
            w.date_us = opt_time(obj, "date").value_or(0);
            w.air_temperature_c = opt_number(obj, "air_temperature").value_or(0.0);
            w.track_temperature_c = opt_number(obj, "track_temperature").value_or(0.0);
            w.humidity_pct = opt_number(obj, "humidity").value_or(0.0);
            w.pressure_mbar = opt_number(obj, "pressure").value_or(0.0);
            w.wind_direction_deg = static_cast<int32_t>(opt_int(obj, "wind_direction").value_or(0));
            w.wind_speed_ms = opt_number(obj, "wind_speed").value_or(0.0);
            w.rainfall = static_cast<int32_t>(opt_int(obj, "rainfall").value_or(0));
            if (w.date_us > 0) result.push_back(w);
        }
    } catch (const std::exception& e) {
        last_error_ = std::string("openf1 weather parse: ") + e.what();
        return {};
    }

    std::sort(result.begin(), result.end(), [](const WeatherSample& a, const WeatherSample& b) {
        return a.date_us < b.date_us;
    });
    return result;
}

} // namespace apex::openf1
