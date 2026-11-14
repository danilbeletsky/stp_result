#pragma once

#include "domain/Types.h"

#include <string>
#include <string_view>

namespace pvz {

// Клиент пункта выдачи. Телефон хранится в нормализованном виде +7XXXXXXXXXX
// и однозначно определяет клиента.
class Customer {
public:
    // Новый клиент, ещё не сохранённый в хранилище (id == 0).
    static Customer create(std::string fullName, std::string_view phone);

    // Восстановление из хранилища. Данные проходят ту же проверку.
    Customer(Id id, std::string fullName, std::string_view phone);

    Id id() const noexcept { return id_; }
    bool isNew() const noexcept { return id_ == 0; }
    const std::string& fullName() const noexcept { return fullName_; }
    const std::string& phone() const noexcept { return phone_; }

    void rename(std::string newName);
    void changePhone(std::string_view newPhone);

    // Обрезает пробелы по краям; имя не пустое и не длиннее 100 символов.
    static std::string normalizeName(std::string_view raw);

    // "+7 (999) 123-45-67", "89991234567", "9991234567" -> "+79991234567".
    // Бросает DomainError, если номер не похож на российский мобильный.
    static std::string normalizePhone(std::string_view raw);

private:
    Id id_;
    std::string fullName_;
    std::string phone_;
};

} // namespace pvz
