#pragma once

#include "domain/Text.h"
#include "domain/Types.h"

#include <ctime>
#include <string>

namespace pvz::cli {

inline std::string formatDateTime(TimePoint moment) {
    const std::time_t seconds = std::chrono::system_clock::to_time_t(moment);
    std::tm local{};
    localtime_r(&seconds, &local);
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%d.%m.%Y %H:%M", &local);
    return buffer;
}

inline std::string formatDate(TimePoint moment) {
    return formatDateTime(moment).substr(0, 10);
}

// Дополняет строку пробелами до нужной ширины в символах (а не байтах),
// чтобы кириллица не ломала выравнивание таблиц.
inline std::string pad(const std::string& value, std::size_t width) {
    const std::size_t length = text::utf8Length(value);
    return length >= width ? value + " " : value + std::string(width - length, ' ');
}

} // namespace pvz::cli
