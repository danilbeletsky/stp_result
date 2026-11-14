#include "repository/postgres/PgCustomerRepository.h"

#include "repository/StorageError.h"

namespace pvz {

namespace {

Customer toCustomer(const pqxx::row& row) {
    return Customer(row["id"].as<Id>(), row["full_name"].as<std::string>(),
                    row["phone"].as<std::string>());
}

} // namespace

Customer PgCustomerRepository::add(const Customer& customer) {
    if (!customer.isNew()) {
        throw StorageError("Клиент уже сохранён (id=" + std::to_string(customer.id()) + ")");
    }
    Id id = 0;
    database_.inTransaction([&](pqxx::work& tx) {
        id = tx.exec("INSERT INTO customers (full_name, phone) VALUES ($1, $2) RETURNING id",
                     pqxx::params{customer.fullName(), customer.phone()})
                 .one_row()[0]
                 .as<Id>();
    });
    return Customer(id, customer.fullName(), customer.phone());
}

std::optional<Customer> PgCustomerRepository::findById(Id id) {
    std::optional<Customer> result;
    database_.inTransaction([&](pqxx::work& tx) {
        auto row = tx.exec("SELECT id, full_name, phone FROM customers WHERE id = $1",
                           pqxx::params{id})
                       .opt_row();
        if (row) {
            result = toCustomer(*row);
        }
    });
    return result;
}

std::optional<Customer> PgCustomerRepository::findByPhone(const std::string& phone) {
    std::optional<Customer> result;
    database_.inTransaction([&](pqxx::work& tx) {
        auto row = tx.exec("SELECT id, full_name, phone FROM customers WHERE phone = $1",
                           pqxx::params{phone})
                       .opt_row();
        if (row) {
            result = toCustomer(*row);
        }
    });
    return result;
}

std::vector<Customer> PgCustomerRepository::findAll() {
    std::vector<Customer> result;
    database_.inTransaction([&](pqxx::work& tx) {
        for (const auto& row : tx.exec("SELECT id, full_name, phone FROM customers "
                                       "ORDER BY full_name, id")) {
            result.push_back(toCustomer(row));
        }
    });
    return result;
}

bool PgCustomerRepository::update(const Customer& customer) {
    bool updated = false;
    database_.inTransaction([&](pqxx::work& tx) {
        auto res = tx.exec("UPDATE customers SET full_name = $1, phone = $2 WHERE id = $3",
                           pqxx::params{customer.fullName(), customer.phone(), customer.id()});
        updated = res.affected_rows() == 1;
    });
    return updated;
}

bool PgCustomerRepository::remove(Id id) {
    bool removed = false;
    database_.inTransaction([&](pqxx::work& tx) {
        removed = tx.exec("DELETE FROM customers WHERE id = $1", pqxx::params{id}).affected_rows() == 1;
    });
    return removed;
}

} // namespace pvz
