#pragma once

#include "domain/Cell.h"
#include "repository/CellRepository.h"
#include "repository/OrderRepository.h"

#include <optional>
#include <string>
#include <vector>

namespace pvz {

struct CellView {
    Cell cell;
    std::optional<std::string> storedTrackingCode;  // что сейчас лежит в ячейке

    bool isFree() const { return !storedTrackingCode.has_value(); }
};

// Справочник ячеек склада.
class CellService {
public:
    CellService(CellRepository& cells, OrderRepository& orders) : cells_(cells), orders_(orders) {}

    Cell addCell(const std::string& code, ParcelSize size);
    std::vector<CellView> listCells();
    // Удалить можно только пустую ячейку.
    void removeCell(const std::string& code);

private:
    CellRepository& cells_;
    OrderRepository& orders_;
};

} // namespace pvz
