#pragma once

#include "domain/Customer.h"

#include <optional>
#include <string>
#include <vector>

namespace pvz {

// Доступ к клиентам. Все методы могут бросать StorageError.
class CustomerRepository {
public:
    virtual ~CustomerRepository() = default;

    // Сохраняет нового клиента и возвращает его с присвоенным id.
    virtual Customer add(const Customer& customer) = 0;
    virtual std::optional<Customer> findById(Id id) = 0;
    virtual std::optional<Customer> findByPhone(const std::string& phone) = 0;
    virtual std::vector<Customer> findAll() = 0;
    // false, если клиента с таким id нет.
    virtual bool update(const Customer& customer) = 0;
    virtual bool remove(Id id) = 0;
};

} // namespace pvz
