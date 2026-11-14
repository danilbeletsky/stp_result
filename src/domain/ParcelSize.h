#pragma once

#include <string>
#include <string_view>

namespace pvz {

// Габарит посылки и ячейки. Посылка помещается в ячейку того же или большего размера.
enum class ParcelSize { Small, Medium, Large };

// 1, 2, 3 — порядок размеров, используется для сравнения и в SQL.
int sizeRank(ParcelSize size) noexcept;

bool fitsInto(ParcelSize parcel, ParcelSize cell) noexcept;

// Код для хранения: "S", "M", "L".
std::string toCode(ParcelSize size);

std::string toDisplayName(ParcelSize size);

// Принимает "S"/"M"/"L" в любом регистре. Бросает DomainError.
ParcelSize parseParcelSize(std::string_view code);

} // namespace pvz
