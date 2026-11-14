#pragma once

#include <stdexcept>
#include <string>

namespace pvz {

// Нарушение правил предметной области: неверный телефон, попытка выдать
// уже выданный заказ, неправильный код получения и т. п.
class DomainError : public std::runtime_error {
public:
    explicit DomainError(const std::string& message) : std::runtime_error(message) {}
};

} // namespace pvz
