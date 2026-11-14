#pragma once

#include <optional>

namespace pvz::pgrow {

// Чтение строк результата, совместимое с libpqxx 7 и 8.
// В libpqxx 8 при переборе результата строки имеют тип pqxx::row_ref, а не pqxx::row,
// поэтому функции преобразования строк в сущности шаблонные. Обращение к полю
// вынесено сюда, чтобы не писать `.template as<T>()` в каждом репозитории.

template <typename T, typename Field>
T get(const Field& field) {
    return field.template as<T>();
}

template <typename T, typename Field>
std::optional<T> getOptional(const Field& field) {
    if (field.is_null()) {
        return std::nullopt;
    }
    return field.template as<T>();
}

} // namespace pvz::pgrow
