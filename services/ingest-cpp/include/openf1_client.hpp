#pragma once

#include "models.hpp"
#include "http_client.hpp"
#include <string>
#include <vector>
#include <optional>
#include <memory>

namespace apex::ingest {

class OpenF1Client {
public:
    explicit OpenF1Client(std::shared_ptr<HttpClient> http_client, std::string base_url = "https://api.openf1.org/v1");

    std::vector<SessionRecord> fetch_sessions(int year = 2024, const std::string& session_type = "");
    std::vector<DriverRecord> fetch_drivers(int64_t session_key);
    std::vector<LapRecord> fetch_laps(int64_t session_key, std::optional<int32_t> driver_number = std::nullopt);
    std::vector<TelemetrySampleRecord> fetch_car_data(int64_t session_key, int32_t driver_number);
    std::vector<RaceControlRecord> fetch_race_control(int64_t session_key);

    const std::string& base_url() const { return base_url_; }

private:
    std::shared_ptr<HttpClient> http_client_;
    std::string base_url_;
};

} // namespace apex::ingest
