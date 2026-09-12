#pragma once

#include <charconv>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <optional>
#include <string>
#include <string_view>

namespace apex::common {

/** Tempo absoluto: microssegundos desde a época Unix, sempre em UTC. */
using MicrosecondsUTC = int64_t;

namespace detail {

inline std::optional<int64_t> parse_int(std::string_view text) {
    int64_t value{};
    const auto* end = text.data() + text.size();
    const auto [ptr, ec] = std::from_chars(text.data(), end, value);
    if (ec != std::errc{} || ptr != end) return std::nullopt;
    return value;
}

/** timegm portátil: converte um tm civil interpretado como UTC em epoch seconds. */
inline int64_t days_from_civil(int64_t y, unsigned m, unsigned d) {
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const auto yoe = static_cast<uint64_t>(y - era * 400);
    const uint64_t doy = (153u * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const uint64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<int64_t>(doe) - 719468;
}

} // namespace detail

/**
 * Converte um timestamp ISO-8601 em microssegundos UTC.
 *
 * Aceita os formatos efetivamente emitidos pela OpenF1 e pelo PostgreSQL:
 *   2024-03-01T16:37:00.084000+00:00
 *   2024-03-01T16:37:00.084Z
 *   2024-03-01 16:37:00+00
 *   2024-03-01T16:37:00
 * Um timestamp sem offset explícito é interpretado como UTC.
 */
inline std::optional<MicrosecondsUTC> parse_iso8601_us(std::string_view text) {
    if (text.size() < 19) return std::nullopt;
    if (text[4] != '-' || text[7] != '-') return std::nullopt;
    if (text[10] != 'T' && text[10] != ' ') return std::nullopt;
    if (text[13] != ':' || text[16] != ':') return std::nullopt;

    const auto year = detail::parse_int(text.substr(0, 4));
    const auto month = detail::parse_int(text.substr(5, 2));
    const auto day = detail::parse_int(text.substr(8, 2));
    const auto hour = detail::parse_int(text.substr(11, 2));
    const auto minute = detail::parse_int(text.substr(14, 2));
    const auto second = detail::parse_int(text.substr(17, 2));
    if (!year || !month || !day || !hour || !minute || !second) return std::nullopt;
    if (*month < 1 || *month > 12 || *day < 1 || *day > 31) return std::nullopt;
    if (*hour > 23 || *minute > 59 || *second > 60) return std::nullopt;

    size_t cursor = 19;
    int64_t fraction_us = 0;
    if (cursor < text.size() && (text[cursor] == '.' || text[cursor] == ',')) {
        ++cursor;
        int digits = 0;
        while (cursor < text.size() && text[cursor] >= '0' && text[cursor] <= '9') {
            if (digits < 6) fraction_us = fraction_us * 10 + (text[cursor] - '0');
            ++digits;
            ++cursor;
        }
        if (digits == 0) return std::nullopt;
        for (int i = digits; i < 6; ++i) fraction_us *= 10;
    }

    int64_t offset_seconds = 0;
    if (cursor < text.size()) {
        const char sign = text[cursor];
        if (sign == 'Z' || sign == 'z') {
            ++cursor;
        } else if (sign == '+' || sign == '-') {
            ++cursor;
            if (cursor + 2 > text.size()) return std::nullopt;
            const auto off_hour = detail::parse_int(text.substr(cursor, 2));
            if (!off_hour) return std::nullopt;
            cursor += 2;
            int64_t off_minute = 0;
            if (cursor < text.size() && text[cursor] == ':') ++cursor;
            if (cursor + 2 <= text.size() && text[cursor] >= '0' && text[cursor] <= '9') {
                const auto parsed = detail::parse_int(text.substr(cursor, 2));
                if (!parsed) return std::nullopt;
                off_minute = *parsed;
                cursor += 2;
            }
            offset_seconds = (*off_hour * 3600 + off_minute * 60) * (sign == '-' ? -1 : 1);
        }
    }

    const int64_t days = detail::days_from_civil(*year, static_cast<unsigned>(*month),
                                                 static_cast<unsigned>(*day));
    const int64_t epoch_seconds =
        days * 86400 + *hour * 3600 + *minute * 60 + *second - offset_seconds;
    return epoch_seconds * 1'000'000 + fraction_us;
}

/** Formata microssegundos UTC como ISO-8601 com precisão de milissegundos e sufixo Z. */
inline std::string format_iso8601_us(MicrosecondsUTC micros) {
    const auto seconds = static_cast<std::time_t>(
        micros >= 0 ? micros / 1'000'000 : (micros - 999'999) / 1'000'000);
    int64_t remainder = micros - static_cast<int64_t>(seconds) * 1'000'000;
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &seconds);
#else
    gmtime_r(&seconds, &tm);
#endif
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
                  static_cast<int>(remainder / 1000));
    return std::string(buffer);
}

} // namespace apex::common
