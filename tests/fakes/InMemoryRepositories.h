#pragma once

// Репозитории в памяти для модульных тестов сервисов.
// Нужны, чтобы проверять бизнес-логику без PostgreSQL — это и показывает,
// что сервисы зависят только от интерфейсов.

#include "repository/CellRepository.h"
#include "repository/CustomerRepository.h"
#include "repository/OrderRepository.h"
#include "repository/StorageError.h"
#include "service/Clock.h"

#include <algorithm>
#include <map>

namespace pvz::test {

// Общие «таблицы»: репозиторию ячеек нужно знать о заказах, как SQL-запросу с JOIN.
struct InMemoryData {
    std::map<Id, Customer> customers;
    std::map<Id, Cell> cells;
    std::map<Id, Order> orders;
    Id nextId = 1;
};

class InMemoryCustomerRepository final : public CustomerRepository {
public:
    explicit InMemoryCustomerRepository(InMemoryData& data) : data_(data) {}

    Customer add(const Customer& customer) override {
        if (findByPhone(customer.phone())) {
            throw DuplicateError("phone");
        }
        Customer saved(data_.nextId++, customer.fullName(), customer.phone());
        data_.customers.emplace(saved.id(), saved);
        return saved;
    }
    std::optional<Customer> findById(Id id) override {
        auto it = data_.customers.find(id);
        return it == data_.customers.end() ? std::nullopt : std::optional<Customer>(it->second);
    }
    std::optional<Customer> findByPhone(const std::string& phone) override {
        for (const auto& [id, customer] : data_.customers) {
            if (customer.phone() == phone) {
                return customer;
            }
        }
        return std::nullopt;
    }
    std::vector<Customer> findAll() override {
        std::vector<Customer> result;
        for (const auto& [id, customer] : data_.customers) {
            result.push_back(customer);
        }
        return result;
    }
    bool update(const Customer& customer) override {
        auto it = data_.customers.find(customer.id());
        if (it == data_.customers.end()) {
            return false;
        }
        it->second = customer;
        return true;
    }
    bool remove(Id id) override { return data_.customers.erase(id) == 1; }

private:
    InMemoryData& data_;
};

class InMemoryCellRepository final : public CellRepository {
public:
    explicit InMemoryCellRepository(InMemoryData& data) : data_(data) {}

    Cell add(const Cell& cell) override {
        if (findByCode(cell.code())) {
            throw DuplicateError("code");
        }
        Cell saved(data_.nextId++, cell.code(), cell.size());
        data_.cells.emplace(saved.id(), saved);
        return saved;
    }
    std::optional<Cell> findById(Id id) override {
        auto it = data_.cells.find(id);
        return it == data_.cells.end() ? std::nullopt : std::optional<Cell>(it->second);
    }
    std::optional<Cell> findByCode(const std::string& code) override {
        for (const auto& [id, cell] : data_.cells) {
            if (cell.code() == code) {
                return cell;
            }
        }
        return std::nullopt;
    }
    std::vector<Cell> findAll() override {
        std::vector<Cell> result;
        for (const auto& [id, cell] : data_.cells) {
            result.push_back(cell);
        }
        return result;
    }
    std::optional<Cell> findFreeCellFor(ParcelSize parcelSize) override {
        std::optional<Cell> best;
        for (const auto& [id, cell] : data_.cells) {
            if (!cell.canHold(parcelSize) || isOccupied(id)) {
                continue;
            }
            if (!best || sizeRank(cell.size()) < sizeRank(best->size()) ||
                (cell.size() == best->size() && cell.code() < best->code())) {
                best = cell;
            }
        }
        return best;
    }
    bool remove(Id id) override { return data_.cells.erase(id) == 1; }

private:
    bool isOccupied(Id cellId) const {
        return std::any_of(data_.orders.begin(), data_.orders.end(), [cellId](const auto& entry) {
            return entry.second.isStored() && entry.second.cellId() == cellId;
        });
    }

    InMemoryData& data_;
};

class InMemoryOrderRepository final : public OrderRepository {
public:
    explicit InMemoryOrderRepository(InMemoryData& data) : data_(data) {}

    Order add(const Order& order) override {
        if (findByTrackingCode(order.trackingCode())) {
            throw DuplicateError("tracking");
        }
        OrderRecord record = toRecord(order);
        record.id = data_.nextId++;
        Order saved = Order::restore(record);
        data_.orders.emplace(saved.id(), saved);
        return saved;
    }
    std::optional<Order> findById(Id id) override {
        auto it = data_.orders.find(id);
        return it == data_.orders.end() ? std::nullopt : std::optional<Order>(it->second);
    }
    std::optional<Order> findByTrackingCode(const std::string& trackingCode) override {
        for (const auto& [id, order] : data_.orders) {
            if (order.trackingCode() == trackingCode) {
                return order;
            }
        }
        return std::nullopt;
    }
    std::vector<Order> findByCustomer(Id customerId) override {
        return filter([customerId](const Order& o) { return o.customerId() == customerId; });
    }
    std::vector<Order> findStored() override {
        return filter([](const Order& o) { return o.isStored(); });
    }
    std::vector<Order> findStoredReceivedBy(TimePoint moment) override {
        return filter([moment](const Order& o) { return o.isStored() && o.receivedAt() <= moment; });
    }
    std::size_t countByCustomer(Id customerId) override { return findByCustomer(customerId).size(); }
    bool updateIfStored(const Order& order) override {
        auto it = data_.orders.find(order.id());
        if (it == data_.orders.end() || !it->second.isStored()) {
            return false;
        }
        it->second = order;
        return true;
    }

    static OrderRecord toRecord(const Order& order) {
        OrderRecord record;
        record.id = order.id();
        record.trackingCode = order.trackingCode();
        record.customerId = order.customerId();
        record.cellId = order.cellId();
        record.parcelSize = order.parcelSize();
        record.pickupCode = order.pickupCode();
        record.status = order.status();
        record.receivedAt = order.receivedAt();
        record.closedAt = order.closedAt();
        return record;
    }

private:
    template <typename Predicate>
    std::vector<Order> filter(Predicate predicate) const {
        std::vector<Order> result;
        for (const auto& [id, order] : data_.orders) {
            if (predicate(order)) {
                result.push_back(order);
            }
        }
        return result;
    }

    InMemoryData& data_;
};

// Часы, которые стоят на месте, пока тест их не передвинет.
class ManualClock final : public Clock {
public:
    explicit ManualClock(TimePoint start) : now_(start) {}
    TimePoint now() const override { return now_; }
    void advance(std::chrono::seconds delta) { now_ += delta; }

private:
    TimePoint now_;
};

} // namespace pvz::test
