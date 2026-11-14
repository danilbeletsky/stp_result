#pragma once

#include "repository/OrderRepository.h"
#include "repository/postgres/PgDatabase.h"

namespace pvz {

class PgOrderRepository final : public OrderRepository {
public:
    explicit PgOrderRepository(PgDatabase& database) : database_(database) {}

    Order add(const Order& order) override;
    std::optional<Order> findById(Id id) override;
    std::optional<Order> findByTrackingCode(const std::string& trackingCode) override;
    std::vector<Order> findByCustomer(Id customerId) override;
    std::vector<Order> findStored() override;
    std::vector<Order> findStoredReceivedBy(TimePoint moment) override;
    std::size_t countByCustomer(Id customerId) override;
    bool updateIfStored(const Order& order) override;

private:
    std::vector<Order> query(const std::string& whereAndOrder, const pqxx::params& params);

    PgDatabase& database_;
};

} // namespace pvz
