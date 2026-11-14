#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>

namespace pvz {

class ConfigError : public std::runtime_error {
public:
    explicit ConfigError(const std::string& message) : std::runtime_error(message) {}
};

struct DatabaseSettings {
    std::string host = "localhost";
    int port = 5432;
    std::string name = "pvz";
    std::string user;      // пусто — libpq возьмёт имя текущего пользователя ОС
    std::string password;  // пусто — без пароля или из ~/.pgpass
};

struct AppConfig {
    DatabaseSettings database;
    int storageDays = 7;            // срок хранения заказа в пункте выдачи
    int checkIntervalSeconds = 60;  // период фоновой проверки просроченных заказов
};

// Источник переменных окружения. В программе это std::getenv, в тестах — словарь.
using EnvironmentLookup = std::function<std::optional<std::string>(const std::string&)>;

EnvironmentLookup systemEnvironment();

// Собирает настройки по приоритету: значения по умолчанию < файл < переменные окружения.
// Файл — строки вида KEY=VALUE, пустые строки и строки с '#' пропускаются.
// Ключи файла совпадают с именами переменных окружения (PVZ_DB_HOST и т. д.).
// Бросает ConfigError при неизвестном ключе, ошибке формата или недопустимом значении.
AppConfig loadConfig(const std::optional<std::filesystem::path>& configFile,
                     const EnvironmentLookup& environment);

} // namespace pvz
