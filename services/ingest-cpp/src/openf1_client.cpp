#include "openf1_client.hpp"
#include "../../common/simdjson/simdjson.h"
#include <iostream>

namespace apex::ingest {

OpenF1Client::OpenF1Client(std::shared_ptr<HttpClient> http_client, std::string base_url)
    : http_client_(std::move(http_client)), base_url_(std::move(base_url)) {}

std::vector<SessionRecord> OpenF1Client::fetch_sessions(int year, const std::string& session_type) {
    std::string url = base_url_ + "/sessions?year=" + std::to_string(year);
    if (!session_type.empty()) {
        url += "&session_name=" + session_type;
    }

    auto resp = http_client_->get(url);
    if (!resp.is_success()) {
        std::cerr << "OpenF1Client error fetching sessions: " << resp.error_message << std::endl;
        return {};
    }

    std::vector<SessionRecord> sessions;
    try {
        simdjson::ondemand::parser parser;
        simdjson::padded_string json_padded(resp.body);
        simdjson::ondemand::document doc = parser.iterate(json_padded);

        for (auto item : doc.get_array()) {
            simdjson::ondemand::object obj = item.get_object();
            SessionRecord s;
            s.session_key = obj["session_key"].get_int64();
            s.session_name = std::string(obj["session_name"].get_string().value_unsafe());
            s.session_type = std::string(obj["session_type"].get_string().value_unsafe());
            s.circuit_key = static_cast<int32_t>(obj["circuit_key"].get_int64());
            s.circuit_name = std::string(obj["circuit_short_name"].get_string().value_unsafe());
            s.country_name = std::string(obj["country_name"].get_string().value_unsafe());
            s.date_start = std::string(obj["date_start"].get_string().value_unsafe());
            s.year = static_cast<int32_t>(obj["year"].get_int64());
            sessions.push_back(std::move(s));
        }
    } catch (const std::exception& e) {
        std::cerr << "Simdjson error parsing sessions: " << e.what() << std::endl;
    }

    return sessions;
}

std::vector<DriverRecord> OpenF1Client::fetch_drivers(int64_t session_key) {
    std::string url = base_url_ + "/drivers?session_key=" + std::to_string(session_key);
    auto resp = http_client_->get(url);
    if (!resp.is_success()) {
        return {};
    }

    std::vector<DriverRecord> drivers;
    try {
        simdjson::ondemand::parser parser;
        simdjson::padded_string json_padded(resp.body);
        simdjson::ondemand::document doc = parser.iterate(json_padded);

        for (auto item : doc.get_array()) {
            simdjson::ondemand::object obj = item.get_object();
            DriverRecord d;
            d.session_key = session_key;
            d.driver_number = static_cast<int32_t>(obj["driver_number"].get_int64());
            d.broadcast_name = std::string(obj["broadcast_name"].get_string().value_unsafe());
            d.full_name = std::string(obj["full_name"].get_string().value_unsafe());
            d.name_acronym = std::string(obj["name_acronym"].get_string().value_unsafe());
            d.team_name = std::string(obj["team_name"].get_string().value_unsafe());
            
            auto colour_res = obj["team_colour"].get_string();
            d.team_colour = colour_res.error() ? "#ffffff" : ("#" + std::string(colour_res.value_unsafe()));

            drivers.push_back(std::move(d));
        }
    } catch (const std::exception& e) {
        std::cerr << "Simdjson error parsing drivers: " << e.what() << std::endl;
    }

    return drivers;
}

std::vector<LapRecord> OpenF1Client::fetch_laps(int64_t session_key, std::optional<int32_t> driver_number) {
    std::string url = base_url_ + "/laps?session_key=" + std::to_string(session_key);
    if (driver_number.has_value()) {
        url += "&driver_number=" + std::to_string(driver_number.value());
    }

    auto resp = http_client_->get(url);
    if (!resp.is_success()) {
        return {};
    }

    std::vector<LapRecord> laps;
    try {
        simdjson::ondemand::parser parser;
        simdjson::padded_string json_padded(resp.body);
        simdjson::ondemand::document doc = parser.iterate(json_padded);

        for (auto item : doc.get_array()) {
            simdjson::ondemand::object obj = item.get_object();
            LapRecord l;
            l.session_key = session_key;
            l.driver_number = static_cast<int32_t>(obj["driver_number"].get_int64());
            l.lap_number = static_cast<int32_t>(obj["lap_number"].get_int64());

            auto lap_dur = obj["lap_duration"].get_double();
            if (!lap_dur.error()) {
                l.lap_duration_us = static_cast<int64_t>(lap_dur.value_unsafe() * 1000000.0);
            }
            l.is_valid = !obj["is_pit_out_lap"].get_bool().value_unsafe();
            l.lap_kind = l.is_valid ? "FLYING" : "OUT_LAP";
            l.coverage_pct = 100.0;
            laps.push_back(std::move(l));
        }
    } catch (const std::exception& e) {
        std::cerr << "Simdjson error parsing laps: " << e.what() << std::endl;
    }

    return laps;
}

std::vector<TelemetrySampleRecord> OpenF1Client::fetch_car_data(int64_t session_key, int32_t driver_number) {
    std::string url = base_url_ + "/car_data?session_key=" + std::to_string(session_key) +
                      "&driver_number=" + std::to_string(driver_number);
    auto resp = http_client_->get(url);
    if (!resp.is_success()) {
        return {};
    }

    std::vector<TelemetrySampleRecord> samples;
    try {
        simdjson::ondemand::parser parser;
        simdjson::padded_string json_padded(resp.body);
        simdjson::ondemand::document doc = parser.iterate(json_padded);

        for (auto item : doc.get_array()) {
            simdjson::ondemand::object obj = item.get_object();
            TelemetrySampleRecord s;
            s.session_key = session_key;
            s.driver_number = driver_number;
            s.speed_kmh = static_cast<double>(obj["speed"].get_int64());
            s.throttle_pct = static_cast<double>(obj["throttle"].get_int64());
            s.brake_pct = static_cast<double>(obj["brake"].get_int64());
            s.rpm = static_cast<int32_t>(obj["rpm"].get_int64());
            s.gear = static_cast<int32_t>(obj["n_gear"].get_int64());
            s.drs = obj["drs"].get_int64().value_unsafe() >= 10;
            samples.push_back(std::move(s));
        }
    } catch (const std::exception& e) {
        std::cerr << "Simdjson error parsing car_data: " << e.what() << std::endl;
    }

    return samples;
}

std::vector<RaceControlRecord> OpenF1Client::fetch_race_control(int64_t session_key) {
    std::string url = base_url_ + "/race_control?session_key=" + std::to_string(session_key);
    auto resp = http_client_->get(url);
    if (!resp.is_success()) {
        return {};
    }

    std::vector<RaceControlRecord> events;
    try {
        simdjson::ondemand::parser parser;
        simdjson::padded_string json_padded(resp.body);
        simdjson::ondemand::document doc = parser.iterate(json_padded);

        for (auto item : doc.get_array()) {
            simdjson::ondemand::object obj = item.get_object();
            RaceControlRecord rc;
            rc.session_key = session_key;
            rc.category = std::string(obj["category"].get_string().value_unsafe());
            
            auto flag_val = obj["flag"].get_string();
            rc.flag = flag_val.error() ? "" : std::string(flag_val.value_unsafe());
            
            rc.message = std::string(obj["message"].get_string().value_unsafe());
            events.push_back(std::move(rc));
        }
    } catch (const std::exception& e) {
        std::cerr << "Simdjson error parsing race_control: " << e.what() << std::endl;
    }

    return events;
}

} // namespace apex::ingest
