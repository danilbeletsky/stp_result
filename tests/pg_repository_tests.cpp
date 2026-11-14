// Интеграционные тесты репозиториев на настоящем PostgreSQL.
// Запускаются, только если задана переменная PVZ_TEST_DB_NAME (отдельная тестовая база!):
// тест удаляет и заново создаёт таблицы в этой базе.
//   createdb pvz_test
//   PVZ_TEST_DB_NAME=pvz_test ctest --test-dir build --output-on-failure

#include "config/Config.h"
#include "repository/StorageError.h"
#include "repository/postgres/PgCellRepository.h"
#include "repository/postgres/PgCustomerRepository.h"
#include "repository/postgres/PgDatabase.h"
#include "repository/postgres/PgOrderRepository.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>
#include <memory>
#include <sstream>

using namespace pvz;
using namespace std::chrono_literals;

namespace {

std::string readFile(const std::string& path) {
    std::ifstream input(path);
    std::stringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

class PgRepositoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        const char* testDb = std::getenv("PVZ_TEST_DB_NAME");
        if (!testDb) {
            GTEST_SKIP() << "PVZ_TEST_DB_NAME не задана — интеграционные тесты пропущены";
        }
        AppConfig config = loadConfig(std::nullopt, systemEnvironment());
        config.database.name = testDb;

        database = std::make_unique<PgDatabase>(config.database);
        const std::string schema = readFile(PVZ_SQL_DIR "/init.sql");
        ASSERT_FALSE(schema.empty());
        database->inTransaction([&](pqxx::work& tx) {
            tx.exec("DROP TABLE IF EXISTS orders, cells, customers CASCADE");
            tx.exec(schema);  // тот же init.sql, что применяется к рабочей базе
        });

        customers = std::make_unique<PgCustomerRepository>(*database);
        cells = std::make_unique<PgCellRepository>(*database);
        orders = std::make_unique<PgOrderRepository>(*database);
    }

    std::unique_ptr<PgDatabase> database;
    std::unique_ptr<PgCustomerRepository> customers;
    std::unique_ptr<PgCellRepository> cells;
    std::unique_ptr<PgOrderRepository> orders;

    const TimePoint now = TimePoint(std::chrono::seconds(1'700'000'000));
};

} // namespace

TEST_F(PgRepositoryTest, CustomerCrud) {
    const Customer saved = customers->add(Customer::create("Иванов Иван", "89161234567"));
    EXPECT_GT(saved.id(), 0);

    auto found = customers->findByPhone("+79161234567");
    ASSERT_TRUE(found);
    EXPECT_EQ(found->fullName(), "Иванов Иван");

    Customer changed = *found;
    changed.rename("Иванов Иван Иванович");
    EXPECT_TRUE(customers->update(changed));
    EXPECT_EQ(customers->findById(saved.id())->fullName(), "Иванов Иван Иванович");

    EXPECT_THROW(customers->add(Customer::create("Дубль", "+79161234567")), DuplicateError);

    EXPECT_TRUE(customers->remove(saved.id()));
    EXPECT_FALSE(customers->findById(saved.id()));
    EXPECT_FALSE(customers->remove(saved.id()));
}

TEST_F(PgRepositoryTest, FreeCellSearchSkipsOccupiedAndTooSmall) {
    const Customer customer = customers->add(Customer::create("Клиент", "+79000000001"));
    const Cell small = cells->add(Cell::create("A-01", ParcelSize::Small));
    const Cell medium = cells->add(Cell::create("B-01", ParcelSize::Medium));

    EXPECT_EQ(cells->findFreeCellFor(ParcelSize::Small)->id(), small.id());
    EXPECT_EQ(cells->findFreeCellFor(ParcelSize::Medium)->id(), medium.id());
    EXPECT_FALSE(cells->findFreeCellFor(ParcelSize::Large));

    orders->add(Order::receive("WB-1", customer.id(), small.id(), ParcelSize::Small, "1234", now));
    EXPECT_EQ(cells->findFreeCellFor(ParcelSize::Small)->id(), medium.id());
}

TEST_F(PgRepositoryTest, OrderRoundTripAndConditionalUpdate) {
    const Customer customer = customers->add(Customer::create("Клиент", "+79000000002"));
    const Cell cell = cells->add(Cell::create("C-01", ParcelSize::Large));

    Order order = orders->add(
        Order::receive("OZ-777", customer.id(), cell.id(), ParcelSize::Medium, "0042", now));
    auto loaded = orders->findByTrackingCode("OZ-777");
    ASSERT_TRUE(loaded);
    EXPECT_EQ(loaded->receivedAt(), now);
    EXPECT_EQ(loaded->pickupCode(), "0042");
    EXPECT_EQ(loaded->cellId(), cell.id());
    EXPECT_EQ(orders->findStoredReceivedBy(now).size(), 1u);
    EXPECT_EQ(orders->findStoredReceivedBy(now - 1s).size(), 0u);

    Order stale = *loaded;  // копия, которую «держит» другой поток
    order.issue("0042", now + 1h, std::chrono::days(7));
    EXPECT_TRUE(orders->updateIfStored(order));

    stale.returnToSender(now + 2h);
    EXPECT_FALSE(orders->updateIfStored(stale));  // выданный заказ не перезаписывается
    const Order final = *orders->findById(order.id());
    EXPECT_EQ(final.status(), OrderStatus::Issued);
    EXPECT_EQ(final.closedAt(), now + 1h);
    EXPECT_EQ(orders->countByCustomer(customer.id()), 1u);
}

TEST_F(PgRepositoryTest, DatabaseProtectsInvariants) {
    const Customer customer = customers->add(Customer::create("Клиент", "+79000000003"));
    const Cell cell = cells->add(Cell::create("D-01", ParcelSize::Small));
    orders->add(Order::receive("T-0001", customer.id(), cell.id(), ParcelSize::Small, "1111", now));

    // Вторая посылка в занятую ячейку — нарушение уникального индекса.
    EXPECT_THROW(orders->add(Order::receive("T-0002", customer.id(), cell.id(), ParcelSize::Small,
                                            "2222", now)),
                 DuplicateError);
    // Клиента с заказами удалить нельзя (ON DELETE RESTRICT).
    EXPECT_THROW(customers->remove(customer.id()), ReferenceError);
}

TEST(PgDatabaseTest, ReportsConnectionFailure) {
    DatabaseSettings settings;
    settings.host = "127.0.0.1";
    settings.port = 1;  // заведомо закрытый порт
    EXPECT_THROW(PgDatabase{settings}, ConnectionError);
}

TEST(PgDatabaseTest, QuotesConnectionStringValues) {
    DatabaseSettings settings;
    settings.user = "it's me";
    settings.password = "p\\w";
    const std::string connection = PgDatabase::makeConnectionString(settings);
    EXPECT_NE(connection.find("user='it\\'s me'"), std::string::npos);
    EXPECT_NE(connection.find("password='p\\\\w'"), std::string::npos);
}
