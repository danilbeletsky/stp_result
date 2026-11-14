#include "service/OrderService.h"

#include "domain/Customer.h"
#include "service/ServiceError.h"

#include <unordered_map>

namespace pvz {

OrderService::OrderService(OrderRepository& orders, CustomerRepository& customers,
                           CellRepository& cells, const Clock& clock,
                           std::chrono::days storagePeriod)
    : orders_(orders),
      customers_(customers),
      cells_(cells),
      clock_(clock),
      storagePeriod_(storagePeriod),
      random_(std::random_device{}()) {
    if (storagePeriod_.count() <= 0) {
        throw ServiceError("Срок хранения должен быть положительным");
    }
}

OrderDetails OrderService::receive(const std::string& trackingCode,
                                   const std::string& customerPhone, ParcelSize parcelSize) {
    const std::string tracking = Order::normalizeTrackingCode(trackingCode);
    if (orders_.findByTrackingCode(tracking)) {
        throw ServiceError("Заказ " + tracking + " уже принимался в пункте выдачи");
    }

    const std::string phone = Customer::normalizePhone(customerPhone);
    auto customer = customers_.findByPhone(phone);
    if (!customer) {
        throw NotFoundError("Клиент с телефоном " + phone + " не зарегистрирован");
    }

    auto cell = cells_.findFreeCellFor(parcelSize);
    if (!cell) {
        throw ServiceError("Нет свободной ячейки для посылки размера " + toDisplayName(parcelSize));
    }

    Order order = Order::receive(tracking, customer->id(), cell->id(), parcelSize,
                                 generatePickupCode(), clock_.now());
    order = orders_.add(order);
    return OrderDetails{order, customer->fullName(), customer->phone(), cell->code(),
                        order.storageDeadline(storagePeriod_)};
}

OrderDetails OrderService::issue(const std::string& trackingCode, const std::string& pickupCode) {
    Order order = loadByTrackingCode(trackingCode);
    order.issue(pickupCode, clock_.now(), storagePeriod_);
    save(order);
    return describe({order}).front();
}

OrderDetails OrderService::returnToSender(const std::string& trackingCode) {
    Order order = loadByTrackingCode(trackingCode);
    order.returnToSender(clock_.now());
    save(order);
    return describe({order}).front();
}

std::size_t OrderService::returnOverdueOrders() {
    const TimePoint now = clock_.now();
    std::size_t returned = 0;
    for (Order& order : orders_.findStoredReceivedBy(now - storagePeriod_)) {
        if (!order.isOverdue(now, storagePeriod_)) {
            continue;
        }
        order.returnToSender(now);
        // Заказ могли выдать, пока мы его обрабатывали, — тогда он просто не обновится.
        if (orders_.updateIfStored(order)) {
            ++returned;
        }
    }
    return returned;
}

OrderDetails OrderService::getByTrackingCode(const std::string& trackingCode) {
    return describe({loadByTrackingCode(trackingCode)}).front();
}

std::vector<OrderDetails> OrderService::ordersOfCustomer(const std::string& customerPhone) {
    const std::string phone = Customer::normalizePhone(customerPhone);
    auto customer = customers_.findByPhone(phone);
    if (!customer) {
        throw NotFoundError("Клиент с телефоном " + phone + " не найден");
    }
    return describe(orders_.findByCustomer(customer->id()));
}

std::vector<OrderDetails> OrderService::storedOrders() {
    return describe(orders_.findStored());
}

Order OrderService::loadByTrackingCode(const std::string& trackingCode) {
    const std::string tracking = Order::normalizeTrackingCode(trackingCode);
    auto order = orders_.findByTrackingCode(tracking);
    if (!order) {
        throw NotFoundError("Заказ " + tracking + " не найден");
    }
    return *order;
}

void OrderService::save(const Order& order) {
    if (!orders_.updateIfStored(order)) {
        throw ServiceError("Заказ " + order.trackingCode() +
                           " уже закрыт другим оператором или фоновой проверкой");
    }
}

std::string OrderService::generatePickupCode() {
    std::uniform_int_distribution<int> digit(0, 9);
    std::string code;
    for (int i = 0; i < 4; ++i) {
        code += static_cast<char>('0' + digit(random_));
    }
    return code;
}

std::vector<OrderDetails> OrderService::describe(const std::vector<Order>& orders) {
    std::unordered_map<Id, std::string> cellCodes;
    for (const Cell& cell : cells_.findAll()) {
        cellCodes.emplace(cell.id(), cell.code());
    }

    std::unordered_map<Id, Customer> customersById;
    std::vector<OrderDetails> result;
    result.reserve(orders.size());
    for (const Order& order : orders) {
        auto customerIt = customersById.find(order.customerId());
        if (customerIt == customersById.end()) {
            auto customer = customers_.findById(order.customerId());
            if (!customer) {
                throw NotFoundError("Клиент заказа " + order.trackingCode() + " не найден");
            }
            customerIt = customersById.emplace(order.customerId(), *customer).first;
        }

        std::string cellCode = "—";
        if (order.cellId()) {
            if (auto it = cellCodes.find(*order.cellId()); it != cellCodes.end()) {
                cellCode = it->second;
            }
        }
        result.push_back(OrderDetails{order, customerIt->second.fullName(),
                                      customerIt->second.phone(), cellCode,
                                      order.storageDeadline(storagePeriod_)});
    }
    return result;
}

} // namespace pvz
