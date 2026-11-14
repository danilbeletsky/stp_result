#pragma once

#include "domain/Cell.h"

#include <optional>
#include <string>
#include <vector>

namespace pvz {

// Доступ к ячейкам хранения. Все методы могут бросать StorageError.
class CellRepository {
public:
    virtual ~CellRepository() = default;

    virtual Cell add(const Cell& cell) = 0;
    virtual std::optional<Cell> findById(Id id) = 0;
    virtual std::optional<Cell> findByCode(const std::string& code) = 0;
    virtual std::vector<Cell> findAll() = 0;

    // Самая маленькая свободная ячейка, в которую помещается посылка.
    // Свободная — та, в которой сейчас нет заказа на хранении.
    virtual std::optional<Cell> findFreeCellFor(ParcelSize parcelSize) = 0;

    virtual bool remove(Id id) = 0;
};

} // namespace pvz
