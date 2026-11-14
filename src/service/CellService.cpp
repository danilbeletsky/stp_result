#include "service/CellService.h"

#include "service/ServiceError.h"

#include <unordered_map>

namespace pvz {

Cell CellService::addCell(const std::string& code, ParcelSize size) {
    Cell cell = Cell::create(code, size);
    if (cells_.findByCode(cell.code())) {
        throw ServiceError("Ячейка " + cell.code() + " уже существует");
    }
    return cells_.add(cell);
}

std::vector<CellView> CellService::listCells() {
    std::unordered_map<Id, std::string> occupied;
    for (const Order& order : orders_.findStored()) {
        if (order.cellId()) {
            occupied[*order.cellId()] = order.trackingCode();
        }
    }

    std::vector<CellView> result;
    for (Cell& cell : cells_.findAll()) {
        CellView view{cell, std::nullopt};
        if (auto it = occupied.find(cell.id()); it != occupied.end()) {
            view.storedTrackingCode = it->second;
        }
        result.push_back(std::move(view));
    }
    return result;
}

void CellService::removeCell(const std::string& code) {
    const std::string normalized = Cell::normalizeCode(code);
    auto cell = cells_.findByCode(normalized);
    if (!cell) {
        throw NotFoundError("Ячейка " + normalized + " не найдена");
    }
    for (const Order& order : orders_.findStored()) {
        if (order.cellId() == cell->id()) {
            throw ServiceError("Ячейка " + normalized + " занята заказом " + order.trackingCode());
        }
    }
    if (!cells_.remove(cell->id())) {
        throw NotFoundError("Ячейка " + normalized + " уже удалена");
    }
}

} // namespace pvz
