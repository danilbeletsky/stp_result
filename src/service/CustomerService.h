#pragma once

#include "domain/Customer.h"
#include "repository/CustomerRepository.h"
#include "repository/OrderRepository.h"

#include <optional>
#include <string>
#include <vector>

namespace pvz {

// Учёт клиентов (полный CRUD). Клиент ищется по телефону — так его называет оператор.
class CustomerService {
public:
    CustomerService(CustomerRepository& customers, OrderRepository& orders)
        : customers_(customers), orders_(orders) {}

    Customer registerCustomer(const std::string& fullName, const std::string& phone);
    std::vector<Customer> listCustomers();
    std::optional<Customer> findByPhone(const std::string& phone);
    Customer getByPhone(const std::string& phone);
    Customer rename(const std::string& phone, const std::string& newName);
    Customer changePhone(const std::string& phone, const std::string& newPhone);
    // Удаляет клиента, только если у него нет ни одного заказа (история нужна для отчётности).
    void remove(const std::string& phone);

private:
    CustomerRepository& customers_;
    OrderRepository& orders_;
};

} // namespace pvz
