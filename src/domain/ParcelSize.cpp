#include "domain/ParcelSize.h"

#include "domain/DomainError.h"
#include "domain/Text.h"

namespace pvz {

int sizeRank(ParcelSize size) noexcept {
    switch (size) {
    case ParcelSize::Small:
        return 1;
    case ParcelSize::Medium:
        return 2;
    case ParcelSize::Large:
        return 3;
    }
    return 0;
}

bool fitsInto(ParcelSize parcel, ParcelSize cell) noexcept {
    return sizeRank(parcel) <= sizeRank(cell);
}

std::string toCode(ParcelSize size) {
    switch (size) {
    case ParcelSize::Small:
        return "S";
    case ParcelSize::Medium:
        return "M";
    case ParcelSize::Large:
        return "L";
    }
    return "?";
}

std::string toDisplayName(ParcelSize size) {
    switch (size) {
    case ParcelSize::Small:
        return "S (малый)";
    case ParcelSize::Medium:
        return "M (средний)";
    case ParcelSize::Large:
        return "L (большой)";
    }
    return "?";
}

ParcelSize parseParcelSize(std::string_view code) {
    const std::string normalized = text::toUpperAscii(text::trim(code));
    if (normalized == "S") {
        return ParcelSize::Small;
    }
    if (normalized == "M") {
        return ParcelSize::Medium;
    }
    if (normalized == "L") {
        return ParcelSize::Large;
    }
    throw DomainError("Размер должен быть S, M или L, получено: '" + std::string(code) + "'");
}

} // namespace pvz
