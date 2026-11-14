#pragma once

#include "config/Config.h"

#include <pqxx/pqxx>

#include <functional>
#include <memory>
#include <string>

namespace pvz {

// Единственная точка, через которую репозитории ходят в PostgreSQL.
// - хранит одно соединение и переподключается, если оно оборвалось;
// - выполняет работу в транзакции и фиксирует её;
// - переводит исключения libpqxx в StorageError и его наследников.
//
// Соединение libpqxx нельзя использовать из нескольких потоков одновременно,
// поэтому каждому потоку нужен свой экземпляр PgDatabase.
class PgDatabase {
public:
    // Сразу открывает соединение; при неудаче бросает ConnectionError.
    explicit PgDatabase(const DatabaseSettings& settings);

    PgDatabase(const PgDatabase&) = delete;
    PgDatabase& operator=(const PgDatabase&) = delete;

    void inTransaction(const std::function<void(pqxx::work&)>& work);

    std::string describe() const;

    // Строка подключения libpq: host=... port=... dbname=... с экранированием значений.
    static std::string makeConnectionString(const DatabaseSettings& settings);

private:
    pqxx::connection& connection();

    DatabaseSettings settings_;
    std::string connectionString_;
    std::unique_ptr<pqxx::connection> connection_;
};

} // namespace pvz
