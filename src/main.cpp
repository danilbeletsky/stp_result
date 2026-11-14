// Точка сборки приложения: читает настройки, создаёт объекты всех слоёв
// и явно передаёт зависимости через конструкторы. Логики предметной области здесь нет.

#include "cli/App.h"
#include "cli/Console.h"
#include "config/Config.h"
#include "repository/StorageError.h"
#include "repository/postgres/PgCellRepository.h"
#include "repository/postgres/PgCustomerRepository.h"
#include "repository/postgres/PgDatabase.h"
#include "repository/postgres/PgOrderRepository.h"
#include "service/CellService.h"
#include "service/Clock.h"
#include "service/CustomerService.h"
#include "service/OrderService.h"
#include "service/PeriodicWorker.h"

#include <filesystem>
#include <iostream>
#include <optional>
#include <string>

namespace {

constexpr int kExitOk = 0;
constexpr int kExitUsage = 1;
constexpr int kExitConfig = 2;
constexpr int kExitDatabase = 3;

const char* const kDefaultConfigFile = "pvz.conf";

// Соединение и репозитории одного потока. libpqxx-соединение нельзя делить между
// потоками, поэтому у меню и у фоновой проверки по своему экземпляру.
struct PgStorage {
    explicit PgStorage(const pvz::DatabaseSettings& settings)
        : database(settings), customers(database), cells(database), orders(database) {}

    pvz::PgDatabase database;
    pvz::PgCustomerRepository customers;
    pvz::PgCellRepository cells;
    pvz::PgOrderRepository orders;
};

void printUsage(const char* program) {
    std::cout << "Использование: " << program << " [--config <файл>]\n\n"
              << "Настройки читаются из файла (по умолчанию ./" << kDefaultConfigFile
              << ", если он есть)\nи переменных окружения, которые имеют приоритет:\n"
              << "  PVZ_DB_HOST, PVZ_DB_PORT, PVZ_DB_NAME, PVZ_DB_USER, PVZ_DB_PASSWORD,\n"
              << "  PVZ_STORAGE_DAYS, PVZ_CHECK_INTERVAL_SEC\n";
}

} // namespace

int main(int argc, char* argv[]) {
    std::optional<std::filesystem::path> configFile;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return kExitOk;
        }
        if (arg == "--config" && i + 1 < argc) {
            configFile = argv[++i];
        } else {
            std::cerr << "Неизвестный аргумент: " << arg << "\n";
            printUsage(argv[0]);
            return kExitUsage;
        }
    }
    if (!configFile && std::filesystem::exists(kDefaultConfigFile)) {
        configFile = kDefaultConfigFile;
    }

    pvz::AppConfig config;
    try {
        config = pvz::loadConfig(configFile, pvz::systemEnvironment());
    } catch (const pvz::ConfigError& error) {
        std::cerr << "Ошибка конфигурации: " << error.what() << "\n";
        return kExitConfig;
    }

    try {
        PgStorage uiStorage(config.database);
        PgStorage workerStorage(config.database);

        const pvz::SystemClock clock;
        const std::chrono::days storagePeriod(config.storageDays);

        pvz::CustomerService customerService(uiStorage.customers, uiStorage.orders);
        pvz::CellService cellService(uiStorage.cells, uiStorage.orders);
        pvz::OrderService orderService(uiStorage.orders, uiStorage.customers, uiStorage.cells,
                                       clock, storagePeriod);

        pvz::OrderService workerOrderService(workerStorage.orders, workerStorage.customers,
                                             workerStorage.cells, clock, storagePeriod);
        pvz::PeriodicWorker overdueWorker(
            [&workerOrderService] { return workerOrderService.returnOverdueOrders(); },
            std::chrono::seconds(config.checkIntervalSeconds));
        overdueWorker.start();

        pvz::cli::Console console(std::cin, std::cout);
        pvz::cli::App app(console, customerService, cellService, orderService, overdueWorker,
                          uiStorage.database.describe());
        try {
            app.run();
        } catch (const pvz::cli::InputClosed&) {
            std::cout << "Ввод завершён, выход.\n";
        }
        overdueWorker.stop();  // остановка и join до разрушения сервисов и соединений
    } catch (const pvz::ConnectionError& error) {
        std::cerr << error.what() << "\n"
                  << "Проверьте, что PostgreSQL запущен и настройки PVZ_DB_* верны (см. README).\n";
        return kExitDatabase;
    } catch (const pvz::StorageError& error) {
        std::cerr << "Ошибка базы данных: " << error.what() << "\n";
        return kExitDatabase;
    } catch (const std::exception& error) {
        std::cerr << "Непредвиденная ошибка: " << error.what() << "\n";
        return kExitDatabase;
    }
    return kExitOk;
}
