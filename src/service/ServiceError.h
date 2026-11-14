#pragma once

#include <stdexcept>
#include <string>

namespace pvz {

// Операцию нельзя выполнить по бизнес-правилам: клиент не найден,
// свободных ячеек нет, у клиента есть заказы и его нельзя удалить и т. п.
class ServiceError : public std::runtime_error {
public:
    explicit ServiceError(const std::string& message) : std::runtime_error(message) {}
};

class NotFoundError : public ServiceError {
public:
    explicit NotFoundError(const std::string& message) : ServiceError(message) {}
};

} // namespace pvz
