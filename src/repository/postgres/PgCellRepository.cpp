#include "repository/postgres/PgCellRepository.h"

#include "repository/StorageError.h"
#include "repository/postgres/PgRow.h"

namespace pvz {

namespace {

// Порядок размеров S < M < L, совпадает с sizeRank() в домене.
constexpr const char* kSizeRankSql = "(CASE c.size WHEN 'S' THEN 1 WHEN 'M' THEN 2 ELSE 3 END)";

// Row — pqxx::row или pqxx::row_ref, см. PgRow.h.
template <typename Row>
Cell toCell(const Row& row) {
    return Cell(pgrow::get<Id>(row["id"]), pgrow::get<std::string>(row["code"]),
                parseParcelSize(pgrow::get<std::string>(row["size"])));
}

} // namespace

Cell PgCellRepository::add(const Cell& cell) {
    if (!cell.isNew()) {
        throw StorageError("Ячейка уже сохранена (id=" + std::to_string(cell.id()) + ")");
    }
    Id id = 0;
    database_.inTransaction([&](pqxx::work& tx) {
        id = tx.exec("INSERT INTO cells (code, size) VALUES ($1, $2) RETURNING id",
                     pqxx::params{cell.code(), toCode(cell.size())})
                 .one_row()[0]
                 .as<Id>();
    });
    return Cell(id, cell.code(), cell.size());
}

std::optional<Cell> PgCellRepository::findById(Id id) {
    std::optional<Cell> result;
    database_.inTransaction([&](pqxx::work& tx) {
        if (auto row = tx.exec("SELECT id, code, size FROM cells WHERE id = $1", pqxx::params{id})
                           .opt_row()) {
            result = toCell(*row);
        }
    });
    return result;
}

std::optional<Cell> PgCellRepository::findByCode(const std::string& code) {
    std::optional<Cell> result;
    database_.inTransaction([&](pqxx::work& tx) {
        if (auto row = tx.exec("SELECT id, code, size FROM cells WHERE code = $1",
                               pqxx::params{code})
                           .opt_row()) {
            result = toCell(*row);
        }
    });
    return result;
}

std::vector<Cell> PgCellRepository::findAll() {
    std::vector<Cell> result;
    database_.inTransaction([&](pqxx::work& tx) {
        for (const auto& row : tx.exec("SELECT id, code, size FROM cells ORDER BY code")) {
            result.push_back(toCell(row));
        }
    });
    return result;
}

std::optional<Cell> PgCellRepository::findFreeCellFor(ParcelSize parcelSize) {
    const std::string query =
        std::string("SELECT c.id, c.code, c.size FROM cells c WHERE ") + kSizeRankSql +
        " >= $1 AND NOT EXISTS (SELECT 1 FROM orders o "
        "                   WHERE o.cell_id = c.id AND o.status = 'stored') "
        "ORDER BY " + kSizeRankSql + ", c.code LIMIT 1";

    std::optional<Cell> result;
    database_.inTransaction([&](pqxx::work& tx) {
        if (auto row = tx.exec(query, pqxx::params{sizeRank(parcelSize)}).opt_row()) {
            result = toCell(*row);
        }
    });
    return result;
}

bool PgCellRepository::remove(Id id) {
    bool removed = false;
    database_.inTransaction([&](pqxx::work& tx) {
        removed = tx.exec("DELETE FROM cells WHERE id = $1", pqxx::params{id}).affected_rows() == 1;
    });
    return removed;
}

} // namespace pvz
