#pragma once

#include "domain/Types.h"

#include <chrono>
#include <cstdint>

namespace pvz::pgtime {

// Время передаётся в PostgreSQL как число секунд Unix (to_timestamp($n))
// и читается обратно через EXTRACT(EPOCH FROM ...)::BIGINT.
inline std::int64_t toEpochSeconds(TimePoint moment) {
    return std::chrono::duration_cast<std::chrono::seconds>(moment.time_since_epoch()).count();
}

inline TimePoint fromEpochSeconds(std::int64_t seconds) {
    return TimePoint(std::chrono::seconds(seconds));
}

} // namespace pvz::pgtime
