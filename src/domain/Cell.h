#pragma once

#include "domain/ParcelSize.h"
#include "domain/Types.h"

#include <string>
#include <string_view>

namespace pvz {

// Ячейка хранения на складе пункта выдачи, например "A-01" размера M.
// Занята ли ячейка, определяется заказами на хранении, а не полем ячейки.
class Cell {
public:
    static Cell create(std::string_view code, ParcelSize size);

    Cell(Id id, std::string_view code, ParcelSize size);

    Id id() const noexcept { return id_; }
    bool isNew() const noexcept { return id_ == 0; }
    const std::string& code() const noexcept { return code_; }
    ParcelSize size() const noexcept { return size_; }

    bool canHold(ParcelSize parcel) const noexcept { return fitsInto(parcel, size_); }

    // Переводит в верхний регистр и проверяет: 1..10 символов из A-Z, 0-9 и '-'.
    static std::string normalizeCode(std::string_view raw);

private:
    Id id_;
    std::string code_;
    ParcelSize size_;
};

} // namespace pvz
