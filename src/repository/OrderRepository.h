#pragma once

#include "domain/Order.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace pvz {

// Доступ к заказам. Все методы могут бросать StorageError.
class OrderRepository {
public:
    virtual ~OrderRepository() = default;

    virtual Order add(const Order& order) = 0;
    virtual std::optional<Order> findById(Id id) = 0;
    virtual std::optional<Order> findByTrackingCode(const std::string& trackingCode) = 0;
    // Все заказы клиента, новые сверху.
    virtual std::vector<Order> findByCustomer(Id customerId) = 0;
    // Заказы на хранении, старые сверху.
    virtual std::vector<Order> findStored() = 0;
    // Заказы на хранении, принятые не позже указанного момента (received_at <= moment).
    virtual std::vector<Order> findStoredReceivedBy(TimePoint moment) = 0;
    virtual std::size_t countByCustomer(Id customerId) = 0;
    // Сохраняет закрытие заказа (статус и дату закрытия), но только если в хранилище
    // заказ всё ещё на хранении. false — его уже закрыл кто-то другой
    // (например, фоновая проверка вернула его, пока оператор выдавал).
    virtual bool updateIfStored(const Order& order) = 0;
};

} // namespace pvz
