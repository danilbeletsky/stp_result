#pragma once

#include "repository/CustomerRepository.h"
#include "repository/postgres/PgDatabase.h"

namespace pvz {

class PgCustomerRepository final : public CustomerRepository {
public:
    explicit PgCustomerRepository(PgDatabase& database) : database_(database) {}

    Customer add(const Customer& customer) override;
    std::optional<Customer> findById(Id id) override;
    std::optional<Customer> findByPhone(const std::string& phone) override;
    std::vector<Customer> findAll() override;
    bool update(const Customer& customer) override;
    bool remove(Id id) override;

private:
    PgDatabase& database_;
};

} // namespace pvz
