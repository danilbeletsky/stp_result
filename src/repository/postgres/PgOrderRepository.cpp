#include "repository/postgres/PgOrderRepository.h"

#include "repository/StorageError.h"
#include "repository/postgres/PgRow.h"
#include "repository/postgres/PgTime.h"

#include <cstdint>

namespace pvz {

namespace {

constexpr const char* kSelectOrders =
    "SELECT id, tracking_code, customer_id, cell_id, parcel_size, pickup_code, status, "
    "       EXTRACT(EPOCH FROM received_at)::BIGINT AS received_at, "
    "       EXTRACT(EPOCH FROM closed_at)::BIGINT AS closed_at "
    "FROM orders ";

// Row — pqxx::row или pqxx::row_ref, см. PgRow.h.
template <typename Row>
Order toOrder(const Row& row) {
    OrderRecord record;
    record.id = pgrow::get<Id>(row["id"]);
    record.trackingCode = pgrow::get<std::string>(row["tracking_code"]);
    record.customerId = pgrow::get<Id>(row["customer_id"]);
    record.cellId = pgrow::getOptional<Id>(row["cell_id"]);
    record.parcelSize = parseParcelSize(pgrow::get<std::string>(row["parcel_size"]));
    record.pickupCode = pgrow::get<std::string>(row["pickup_code"]);
    record.status = parseOrderStatus(pgrow::get<std::string>(row["status"]));
    record.receivedAt = pgtime::fromEpochSeconds(pgrow::get<std::int64_t>(row["received_at"]));
    if (auto closed = pgrow::getOptional<std::int64_t>(row["closed_at"])) {
        record.closedAt = pgtime::fromEpochSeconds(*closed);
    }
    return Order::restore(std::move(record));
}

std::optional<std::int64_t> closedAtSeconds(const Order& order) {
    if (order.closedAt()) {
        return pgtime::toEpochSeconds(*order.closedAt());
    }
    return std::nullopt;
}

} // namespace

std::vector<Order> PgOrderRepository::query(const std::string& whereAndOrder,
                                            const pqxx::params& params) {
    std::vector<Order> result;
    database_.inTransaction([&](pqxx::work& tx) {
        for (const auto& row : tx.exec(std::string(kSelectOrders) + whereAndOrder, params)) {
            result.push_back(toOrder(row));
        }
    });
    return result;
}

Order PgOrderRepository::add(const Order& order) {
    if (!order.isNew()) {
        throw StorageError("Заказ уже сохранён (id=" + std::to_string(order.id()) + ")");
    }
    Id id = 0;
    database_.inTransaction([&](pqxx::work& tx) {
        id = tx.exec("INSERT INTO orders (tracking_code, customer_id, cell_id, parcel_size, "
                     "                    pickup_code, status, received_at, closed_at) "
                     "VALUES ($1, $2, $3, $4, $5, $6, to_timestamp($7), to_timestamp($8)) "
                     "RETURNING id",
                     pqxx::params{order.trackingCode(), order.customerId(), order.cellId(),
                                  toCode(order.parcelSize()), order.pickupCode(),
                                  toCode(order.status()), pgtime::toEpochSeconds(order.receivedAt()),
                                  closedAtSeconds(order)})
                 .one_row()[0]
                 .as<Id>();
    });

    OrderRecord record;
    record.id = id;
    record.trackingCode = order.trackingCode();
    record.customerId = order.customerId();
    record.cellId = order.cellId();
    record.parcelSize = order.parcelSize();
    record.pickupCode = order.pickupCode();
    record.status = order.status();
    record.receivedAt = order.receivedAt();
    record.closedAt = order.closedAt();
    return Order::restore(std::move(record));
}

std::optional<Order> PgOrderRepository::findById(Id id) {
    auto orders = query("WHERE id = $1", pqxx::params{id});
    if (orders.empty()) {
        return std::nullopt;
    }
    return orders.front();
}

std::optional<Order> PgOrderRepository::findByTrackingCode(const std::string& trackingCode) {
    auto orders = query("WHERE tracking_code = $1", pqxx::params{trackingCode});
    if (orders.empty()) {
        return std::nullopt;
    }
    return orders.front();
}

std::vector<Order> PgOrderRepository::findByCustomer(Id customerId) {
    return query("WHERE customer_id = $1 ORDER BY received_at DESC, id DESC",
                 pqxx::params{customerId});
}

std::vector<Order> PgOrderRepository::findStored() {
    return query("WHERE status = 'stored' ORDER BY received_at, id", pqxx::params{});
}

std::vector<Order> PgOrderRepository::findStoredReceivedBy(TimePoint moment) {
    return query("WHERE status = 'stored' AND received_at <= to_timestamp($1) "
                 "ORDER BY received_at, id",
                 pqxx::params{pgtime::toEpochSeconds(moment)});
}

std::size_t PgOrderRepository::countByCustomer(Id customerId) {
    std::int64_t count = 0;
    database_.inTransaction([&](pqxx::work& tx) {
        count = tx.exec("SELECT COUNT(*) FROM orders WHERE customer_id = $1",
                        pqxx::params{customerId})
                    .one_row()[0]
                    .as<std::int64_t>();
    });
    return static_cast<std::size_t>(count);
}

bool PgOrderRepository::updateIfStored(const Order& order) {
    bool updated = false;
    database_.inTransaction([&](pqxx::work& tx) {
        auto res = tx.exec("UPDATE orders SET status = $1, closed_at = to_timestamp($2) "
                           "WHERE id = $3 AND status = 'stored'",
                           pqxx::params{toCode(order.status()), closedAtSeconds(order), order.id()});
        updated = res.affected_rows() == 1;
    });
    return updated;
}

} // namespace pvz
