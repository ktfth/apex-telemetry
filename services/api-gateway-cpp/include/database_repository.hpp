#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace apex::gateway {

class DatabaseRepository {
public:
    DatabaseRepository();

    bool configured() const;
    bool healthy() const;
    std::optional<std::string> sessions(std::optional<int> year) const;
    std::optional<std::string> drivers(int64_t session_key) const;
    std::optional<std::string> laps(int64_t session_key, std::optional<int32_t> driver_number) const;
    std::optional<std::string> race_control(int64_t session_key) const;

private:
    std::string connection_string_;
    std::optional<std::string> query_json(
        const std::string& sql,
        const std::vector<std::string>& parameters = {}
    ) const;
};

} // namespace apex::gateway
