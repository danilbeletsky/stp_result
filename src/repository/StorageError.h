#pragma once

#include <stdexcept>
#include <string>

namespace pvz {

// Ошибки слоя хранения. Конкретная реализация (PostgreSQL) переводит в них
// исключения своей библиотеки, поэтому сервисы и интерфейс не знают про libpqxx.
class StorageError : public std::runtime_error {
public:
    explicit StorageError(const std::string& message) : std::runtime_error(message) {}
};

// Нет связи с хранилищем (сервер недоступен, соединение оборвалось).
class ConnectionError : public StorageError {
public:
    explicit ConnectionError(const std::string& message) : StorageError(message) {}
};

// Нарушено ограничение уникальности: такой телефон / трек-номер / код ячейки уже есть.
class DuplicateError : public StorageError {
public:
    explicit DuplicateError(const std::string& message) : StorageError(message) {}
};

// Нарушена ссылочная целостность: например, удаляется клиент, у которого есть заказы.
class ReferenceError : public StorageError {
public:
    explicit ReferenceError(const std::string& message) : StorageError(message) {}
};

} // namespace pvz
