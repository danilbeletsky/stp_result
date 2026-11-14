#include "repository/postgres/PgDatabase.h"

#include "repository/StorageError.h"

#include <sstream>

namespace pvz {

namespace {

// libpq допускает значения в одинарных кавычках; внутри экранируются ' и \.
std::string quote(const std::string& value) {
    std::string result = "'";
    for (char c : value) {
        if (c == '\'' || c == '\\') {
            result += '\\';
        }
        result += c;
    }
    result += '\'';
    return result;
}

// Первая строка сообщения сервера без хвоста с текстом запроса.
std::string shortMessage(const std::exception& error) {
    std::string message = error.what();
    if (auto pos = message.find('\n'); pos != std::string::npos) {
        message.erase(pos);
    }
    return message;
}

} // namespace

PgDatabase::PgDatabase(const DatabaseSettings& settings)
    : settings_(settings), connectionString_(makeConnectionString(settings)) {
    connection();
}

std::string PgDatabase::makeConnectionString(const DatabaseSettings& settings) {
    std::ostringstream out;
    out << "host=" << quote(settings.host) << " port=" << settings.port
        << " dbname=" << quote(settings.name) << " connect_timeout=5"
        << " application_name='pvz'";
    if (!settings.user.empty()) {
        out << " user=" << quote(settings.user);
    }
    if (!settings.password.empty()) {
        out << " password=" << quote(settings.password);
    }
    return out.str();
}

std::string PgDatabase::describe() const {
    std::string result = settings_.host + ":" + std::to_string(settings_.port) + "/" + settings_.name;
    if (!settings_.user.empty()) {
        result = settings_.user + "@" + result;
    }
    return result;
}

pqxx::connection& PgDatabase::connection() {
    if (connection_ && connection_->is_open()) {
        return *connection_;
    }
    connection_.reset();
    try {
        connection_ = std::make_unique<pqxx::connection>(connectionString_);
    } catch (const pqxx::broken_connection& error) {
        throw ConnectionError("Не удалось подключиться к PostgreSQL (" + describe() +
                              "): " + shortMessage(error));
    } catch (const std::exception& error) {
        throw ConnectionError("Ошибка подключения к PostgreSQL (" + describe() +
                              "): " + shortMessage(error));
    }
    return *connection_;
}

void PgDatabase::inTransaction(const std::function<void(pqxx::work&)>& work) {
    pqxx::connection& conn = connection();
    try {
        pqxx::work transaction(conn);
        work(transaction);
        transaction.commit();
    } catch (const pqxx::broken_connection& error) {
        connection_.reset();  // в следующий раз переподключимся
        throw ConnectionError("Соединение с PostgreSQL потеряно: " + shortMessage(error));
    } catch (const pqxx::unique_violation& error) {
        throw DuplicateError("Запись с такими данными уже существует: " + shortMessage(error));
    } catch (const pqxx::foreign_key_violation& error) {
        throw ReferenceError("Запись связана с другими данными: " + shortMessage(error));
    } catch (const pqxx::undefined_table& error) {
        throw StorageError("В базе нет нужных таблиц, примените sql/init.sql: " +
                           shortMessage(error));
    } catch (const pqxx::sql_error& error) {
        throw StorageError("Ошибка SQL: " + shortMessage(error));
    } catch (const pqxx::failure& error) {
        throw StorageError("Ошибка работы с PostgreSQL: " + shortMessage(error));
    }
}

} // namespace pvz
