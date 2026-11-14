#pragma once

#include "domain/Types.h"

namespace pvz {

// Источник текущего времени. Сервис не вызывает system_clock напрямую,
// чтобы в тестах можно было «перемотать» время и проверить сроки хранения.
class Clock {
public:
    virtual ~Clock() = default;
    virtual TimePoint now() const = 0;
};

class SystemClock final : public Clock {
public:
    TimePoint now() const override { return std::chrono::system_clock::now(); }
};

} // namespace pvz
