#include "service/CustomerService.h"

#include "service/ServiceError.h"

namespace pvz {

Customer CustomerService::registerCustomer(const std::string& fullName, const std::string& phone) {
    Customer customer = Customer::create(fullName, phone);
    if (customers_.findByPhone(customer.phone())) {
        throw ServiceError("Клиент с телефоном " + customer.phone() + " уже зарегистрирован");
    }
    return customers_.add(customer);
}

std::vector<Customer> CustomerService::listCustomers() {
    return customers_.findAll();
}

std::optional<Customer> CustomerService::findByPhone(const std::string& phone) {
    return customers_.findByPhone(Customer::normalizePhone(phone));
}

Customer CustomerService::getByPhone(const std::string& phone) {
    const std::string normalized = Customer::normalizePhone(phone);
    auto customer = customers_.findByPhone(normalized);
    if (!customer) {
        throw NotFoundError("Клиент с телефоном " + normalized + " не найден");
    }
    return *customer;
}

Customer CustomerService::rename(const std::string& phone, const std::string& newName) {
    Customer customer = getByPhone(phone);
    customer.rename(newName);
    if (!customers_.update(customer)) {
        throw NotFoundError("Клиент был удалён во время изменения");
    }
    return customer;
}

Customer CustomerService::changePhone(const std::string& phone, const std::string& newPhone) {
    Customer customer = getByPhone(phone);
    customer.changePhone(newPhone);
    if (auto other = customers_.findByPhone(customer.phone()); other && other->id() != customer.id()) {
        throw ServiceError("Телефон " + customer.phone() + " уже принадлежит клиенту " +
                           other->fullName());
    }
    if (!customers_.update(customer)) {
        throw NotFoundError("Клиент был удалён во время изменения");
    }
    return customer;
}

void CustomerService::remove(const std::string& phone) {
    const Customer customer = getByPhone(phone);
    const std::size_t orderCount = orders_.countByCustomer(customer.id());
    if (orderCount > 0) {
        throw ServiceError("Нельзя удалить клиента " + customer.fullName() + ": у него " +
                           std::to_string(orderCount) + " заказ(ов) в истории");
    }
    if (!customers_.remove(customer.id())) {
        throw NotFoundError("Клиент уже удалён");
    }
}

} // namespace pvz
