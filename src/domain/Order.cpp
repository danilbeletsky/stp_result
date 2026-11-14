#include "domain/Order.h"

#include "domain/DomainError.h"
#include "domain/Text.h"

#include <cctype>
#include <utility>

namespace pvz {

std::string toCode(OrderStatus status) {
    switch (status) {
    case OrderStatus::Stored:
        return "stored";
    case OrderStatus::Issued:
        return "issued";
    case OrderStatus::Returned:
        return "returned";
    }
    return "?";
}

std::string toDisplayName(OrderStatus status) {
    switch (status) {
    case OrderStatus::Stored:
        return "на хранении";
    case OrderStatus::Issued:
        return "выдан";
    case OrderStatus::Returned:
        return "возвращён отправителю";
    }
    return "?";
}

OrderStatus parseOrderStatus(std::string_view code) {
    if (code == "stored") {
        return OrderStatus::Stored;
    }
    if (code == "issued") {
        return OrderStatus::Issued;
    }
    if (code == "returned") {
        return OrderStatus::Returned;
    }
    throw DomainError("Неизвестный статус заказа: '" + std::string(code) + "'");
}

Order Order::receive(std::string_view trackingCode, Id customerId, Id cellId,
                     ParcelSize parcelSize, std::string_view pickupCode, TimePoint now) {
    OrderRecord record;
    record.trackingCode = std::string(trackingCode);
    record.customerId = customerId;
    record.cellId = cellId;
    record.parcelSize = parcelSize;
    record.pickupCode = std::string(pickupCode);
    record.status = OrderStatus::Stored;
    record.receivedAt = now;
    return Order(std::move(record));
}

Order Order::restore(OrderRecord record) {
    return Order(std::move(record));
}

Order::Order(OrderRecord record)
    : id_(record.id),
      trackingCode_(normalizeTrackingCode(record.trackingCode)),
      customerId_(record.customerId),
      cellId_(record.cellId),
      parcelSize_(record.parcelSize),
      pickupCode_(std::move(record.pickupCode)),
      status_(record.status),
      receivedAt_(record.receivedAt),
      closedAt_(record.closedAt) {
    if (id_ < 0) {
        throw DomainError("Идентификатор заказа не может быть отрицательным");
    }
    if (customerId_ <= 0) {
        throw DomainError("Заказ должен принадлежать сохранённому клиенту");
    }
    if (!isValidPickupCode(pickupCode_)) {
        throw DomainError("Код получения должен состоять из 4 цифр");
    }
    if (isStored()) {
        if (!cellId_ || *cellId_ <= 0) {
            throw DomainError("Заказ на хранении должен лежать в ячейке");
        }
        if (closedAt_) {
            throw DomainError("У заказа на хранении не может быть даты закрытия");
        }
    } else if (!closedAt_) {
        throw DomainError("У закрытого заказа должна быть дата закрытия");
    }
}

TimePoint Order::storageDeadline(std::chrono::days storagePeriod) const noexcept {
    return receivedAt_ + storagePeriod;
}

bool Order::isOverdue(TimePoint now, std::chrono::days storagePeriod) const noexcept {
    return isStored() && now >= storageDeadline(storagePeriod);
}

void Order::issue(std::string_view enteredCode, TimePoint now, std::chrono::days storagePeriod) {
    ensureStored("выдать");
    if (isOverdue(now, storagePeriod)) {
        throw DomainError("Срок хранения заказа " + trackingCode_ +
                          " истёк, его нужно вернуть отправителю");
    }
    if (text::trim(enteredCode) != pickupCode_) {
        throw DomainError("Неверный код получения");
    }
    status_ = OrderStatus::Issued;
    closedAt_ = now;
}

void Order::returnToSender(TimePoint now) {
    ensureStored("вернуть отправителю");
    status_ = OrderStatus::Returned;
    closedAt_ = now;
}

void Order::ensureStored(std::string_view action) const {
    if (!isStored()) {
        throw DomainError("Нельзя " + std::string(action) + " заказ " + trackingCode_ +
                          ": он уже " + toDisplayName(status_));
    }
}

std::string Order::normalizeTrackingCode(std::string_view raw) {
    const std::string code = text::toUpperAscii(text::trim(raw));
    if (code.size() < 4 || code.size() > 30) {
        throw DomainError("Трек-номер должен содержать от 4 до 30 символов");
    }
    for (char c : code) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-') {
            throw DomainError("Трек-номер может содержать только латинские буквы, цифры и '-'");
        }
    }
    return code;
}

bool Order::isValidPickupCode(std::string_view code) noexcept {
    if (code.size() != 4) {
        return false;
    }
    for (char c : code) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    return true;
}

} // namespace pvz
