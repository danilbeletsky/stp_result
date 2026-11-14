#include "domain/Cell.h"

#include "domain/DomainError.h"
#include "domain/Text.h"

#include <cctype>

namespace pvz {

namespace {
constexpr std::size_t kMaxCodeLength = 10;
}

Cell Cell::create(std::string_view code, ParcelSize size) {
    return Cell(0, code, size);
}

Cell::Cell(Id id, std::string_view code, ParcelSize size)
    : id_(id), code_(normalizeCode(code)), size_(size) {
    if (id < 0) {
        throw DomainError("Идентификатор ячейки не может быть отрицательным");
    }
}

std::string Cell::normalizeCode(std::string_view raw) {
    const std::string code = text::toUpperAscii(text::trim(raw));
    if (code.empty() || code.size() > kMaxCodeLength) {
        throw DomainError("Код ячейки должен содержать от 1 до 10 символов");
    }
    bool hasAlnum = false;
    for (char c : code) {
        const auto uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc)) {
            hasAlnum = true;
        } else if (c != '-') {
            throw DomainError("Код ячейки может содержать только латинские буквы, цифры и '-'");
        }
    }
    if (!hasAlnum) {
        throw DomainError("Код ячейки должен содержать хотя бы одну букву или цифру");
    }
    return code;
}

} // namespace pvz
