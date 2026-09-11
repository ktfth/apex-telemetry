#pragma once

#include "models.hpp"
#include <string>
#include <vector>

namespace apex::ingest {

class PostgreSQLStorage {
public:
    PostgreSQLStorage();
    bool configured() const;
    bool healthy() const;
    bool upsert_sessions(const std::vector<SessionRecord>& sessions) const;
    bool upsert_drivers(const std::vector<DriverRecord>& drivers) const;

private:
    std::string connection_string_;
};

} // namespace apex::ingest
