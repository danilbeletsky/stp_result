#pragma once

#include "domain/Order.h"
#include "repository/CellRepository.h"
#include "repository/CustomerRepository.h"
#include "repository/OrderRepository.h"
#include "service/Clock.h"

#include <chrono>
#include <cstddef>
#include <random>
#include <string>
#include <vector>

namespace pvz {

// Заказ вместе с данными, которые нужны оператору на экране.
struct OrderDetails {
    Order order;
    std::string customerName;
    std::string customerPhone;
    std::string cellCode;  // "—", если ячейка удалена из справочника
    TimePoint storageDeadline;
};

// Основные операции пункта выдачи: приём, выдача, возврат заказов.
//
// Сервис работает только с интерфейсами репозиториев и часов и ничего не знает
// ни о PostgreSQL, ни о консоли.
// Экземпляр не потокобезопасен: для фонового потока создаётся отдельный экземпляр.
class OrderService {
public:
    OrderService(OrderRepository& orders, CustomerRepository& customers, CellRepository& cells,
                 const Clock& clock, std::chrono::days storagePeriod);

    // Принимает посылку для зарегистрированного клиента: подбирает самую маленькую
    // подходящую свободную ячейку и генерирует 4-значный код получения.
    OrderDetails receive(const std::string& trackingCode, const std::string& customerPhone,
                         ParcelSize parcelSize);

    // Выдаёт заказ по трек-номеру и коду из SMS.
    OrderDetails issue(const std::string& trackingCode, const std::string& pickupCode);

    // Возврат отправителю по отказу клиента.
    OrderDetails returnToSender(const std::string& trackingCode);

    // Возвращает все заказы с истёкшим сроком хранения. Результат — сколько вернули.
    std::size_t returnOverdueOrders();

    OrderDetails getByTrackingCode(const std::string& trackingCode);
    std::vector<OrderDetails> ordersOfCustomer(const std::string& customerPhone);
    std::vector<OrderDetails> storedOrders();

    std::chrono::days storagePeriod() const noexcept { return storagePeriod_; }

private:
    Order loadByTrackingCode(const std::string& trackingCode);
    void save(const Order& order);
    std::string generatePickupCode();
    std::vector<OrderDetails> describe(const std::vector<Order>& orders);

    OrderRepository& orders_;
    CustomerRepository& customers_;
    CellRepository& cells_;
    const Clock& clock_;
    std::chrono::days storagePeriod_;
    std::mt19937 random_;
};

} // namespace pvz
