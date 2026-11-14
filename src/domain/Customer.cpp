#include "domain/Customer.h"

#include "domain/DomainError.h"
#include "domain/Text.h"

#include <cctype>
#include <utility>

namespace pvz {

namespace {
constexpr std::size_t kMaxNameLength = 100;
}

std::string Customer::normalizeName(std::string_view raw) {
    std::string name = text::trim(raw);
    if (name.empty()) {
        throw DomainError("Имя клиента не может быть пустым");
    }
    if (text::utf8Length(name) > kMaxNameLength) {
        throw DomainError("Имя клиента длиннее " + std::to_string(kMaxNameLength) + " символов");
    }
    return name;
}

Customer Customer::create(std::string fullName, std::string_view phone) {
    return Customer(0, std::move(fullName), phone);
}

Customer::Customer(Id id, std::string fullName, std::string_view phone)
    : id_(id), fullName_(normalizeName(fullName)), phone_(normalizePhone(phone)) {
    if (id < 0) {
        throw DomainError("Идентификатор клиента не может быть отрицательным");
    }
}

void Customer::rename(std::string newName) {
    fullName_ = normalizeName(newName);
}

void Customer::changePhone(std::string_view newPhone) {
    phone_ = normalizePhone(newPhone);
}

std::string Customer::normalizePhone(std::string_view raw) {
    std::string digits;
    bool hasPlus = false;
    for (char c : raw) {
        if (std::isdigit(static_cast<unsigned char>(c))) {
            digits += c;
        } else if (c == '+' && digits.empty() && !hasPlus) {
            hasPlus = true;
        } else if (c == ' ' || c == '-' || c == '(' || c == ')') {
            continue;
        } else {
            throw DomainError("Телефон может содержать только цифры, пробелы, скобки, дефис и '+'");
        }
    }

    if (digits.size() == 11 && (digits[0] == '7' || (digits[0] == '8' && !hasPlus))) {
        digits.erase(0, 1);
    } else if (!(digits.size() == 10 && !hasPlus)) {
        throw DomainError("Телефон должен быть в формате +7XXXXXXXXXX или 8XXXXXXXXXX");
    }
    return "+7" + digits;
}

} // namespace pvz
