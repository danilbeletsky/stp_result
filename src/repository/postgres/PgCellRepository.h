#pragma once

#include "repository/CellRepository.h"
#include "repository/postgres/PgDatabase.h"

namespace pvz {

class PgCellRepository final : public CellRepository {
public:
    explicit PgCellRepository(PgDatabase& database) : database_(database) {}

    Cell add(const Cell& cell) override;
    std::optional<Cell> findById(Id id) override;
    std::optional<Cell> findByCode(const std::string& code) override;
    std::vector<Cell> findAll() override;
    std::optional<Cell> findFreeCellFor(ParcelSize parcelSize) override;
    bool remove(Id id) override;

private:
    PgDatabase& database_;
};

} // namespace pvz
