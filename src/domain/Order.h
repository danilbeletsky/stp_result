#pragma once

#include "domain/ParcelSize.h"
#include "domain/Types.h"

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace pvz {

enum class OrderStatus {
    Stored,   // лежит в ячейке и ждёт клиента
    Issued,   // выдан клиенту
    Returned  // возвращён отправителю (истёк срок хранения или отказ)
};

std::string toCode(OrderStatus status);
std::string toDisplayName(OrderStatus status);
OrderStatus parseOrderStatus(std::string_view code);

// Все поля заказа в «плоском» виде. Нужна только для восстановления заказа
// из хранилища; менять состояние существующего заказа можно лишь методами Order.
struct OrderRecord {
    Id id = 0;
    std::string trackingCode;
    Id customerId = 0;
    std::optional<Id> cellId;
    ParcelSize parcelSize = ParcelSize::Small;
    std::string pickupCode;
    OrderStatus status = OrderStatus::Stored;
    TimePoint receivedAt;
    std::optional<TimePoint> closedAt;
};

// Заказ, пришедший в пункт выдачи.
// Жизненный цикл: Stored -> Issued или Stored -> Returned. Закрытый заказ не меняется.
class Order {
public:
    static Order receive(std::string_view trackingCode, Id customerId, Id cellId,
                         ParcelSize parcelSize, std::string_view pickupCode, TimePoint now);

    // Проверяет согласованность данных и бросает DomainError, если они противоречивы.
    static Order restore(OrderRecord record);

    Id id() const noexcept { return id_; }
    bool isNew() const noexcept { return id_ == 0; }
    const std::string& trackingCode() const noexcept { return trackingCode_; }
    Id customerId() const noexcept { return customerId_; }
    // Пусто, если ячейку уже удалили из справочника (заказ давно закрыт).
    const std::optional<Id>& cellId() const noexcept { return cellId_; }
    ParcelSize parcelSize() const noexcept { return parcelSize_; }
    const std::string& pickupCode() const noexcept { return pickupCode_; }
    OrderStatus status() const noexcept { return status_; }
    TimePoint receivedAt() const noexcept { return receivedAt_; }
    const std::optional<TimePoint>& closedAt() const noexcept { return closedAt_; }

    bool isStored() const noexcept { return status_ == OrderStatus::Stored; }

    TimePoint storageDeadline(std::chrono::days storagePeriod) const noexcept;
    bool isOverdue(TimePoint now, std::chrono::days storagePeriod) const noexcept;

    // Выдача клиенту: заказ должен быть на хранении, срок не истёк, код совпадает.
    void issue(std::string_view enteredCode, TimePoint now, std::chrono::days storagePeriod);

    // Возврат отправителю: по истечении срока или по отказу клиента.
    void returnToSender(TimePoint now);

    // Трек-номер: 4..30 символов A-Z, 0-9, '-'; приводится к верхнему регистру.
    static std::string normalizeTrackingCode(std::string_view raw);
    static bool isValidPickupCode(std::string_view code) noexcept;

private:
    explicit Order(OrderRecord record);
    void ensureStored(std::string_view action) const;

    Id id_;
    std::string trackingCode_;
    Id customerId_;
    std::optional<Id> cellId_;
    ParcelSize parcelSize_;
    std::string pickupCode_;
    OrderStatus status_;
    TimePoint receivedAt_;
    std::optional<TimePoint> closedAt_;
};

} // namespace pvz
