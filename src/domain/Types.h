#pragma once

#include <chrono>
#include <cstdint>

namespace pvz {

// Идентификатор записи. 0 означает «ещё не сохранено в хранилище».
using Id = std::int64_t;

using TimePoint = std::chrono::system_clock::time_point;

} // namespace pvz
