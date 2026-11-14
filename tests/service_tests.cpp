#include "domain/DomainError.h"
#include "fakes/InMemoryRepositories.h"
#include "service/CellService.h"
#include "service/CustomerService.h"
#include "service/OrderService.h"
#include "service/ServiceError.h"

#include <gtest/gtest.h>

using namespace pvz;
using namespace std::chrono_literals;

namespace {

constexpr std::chrono::days kStorage{7};

class ServicesTest : public ::testing::Test {
protected:
    test::InMemoryData data;
    test::InMemoryCustomerRepository customerRepo{data};
    test::InMemoryCellRepository cellRepo{data};
    test::InMemoryOrderRepository orderRepo{data};
    test::ManualClock clock{TimePoint(std::chrono::seconds(1'700'000'000))};

    CustomerService customers{customerRepo, orderRepo};
    CellService cells{cellRepo, orderRepo};
    OrderService orders{orderRepo, customerRepo, cellRepo, clock, kStorage};

    void SetUp() override {
        customers.registerCustomer("Иванов Иван", "+79161234567");
        cells.addCell("A-01", ParcelSize::Small);
        cells.addCell("B-01", ParcelSize::Medium);
        cells.addCell("C-01", ParcelSize::Large);
    }
};

} // namespace

TEST_F(ServicesTest, RegisterRejectsDuplicatePhone) {
    EXPECT_THROW(customers.registerCustomer("Другой", "8 916 123-45-67"), ServiceError);
    EXPECT_EQ(customers.listCustomers().size(), 1u);
}

TEST_F(ServicesTest, CustomerCrud) {
    customers.registerCustomer("Петрова Анна", "89035550101");
    EXPECT_EQ(customers.listCustomers().size(), 2u);

    customers.rename("+79035550101", "Петрова-Смирнова Анна");
    EXPECT_EQ(customers.getByPhone("89035550101").fullName(), "Петрова-Смирнова Анна");

    customers.changePhone("+79035550101", "+79035550202");
    EXPECT_FALSE(customers.findByPhone("+79035550101"));
    EXPECT_TRUE(customers.findByPhone("+79035550202"));

    EXPECT_THROW(customers.changePhone("+79035550202", "+79161234567"), ServiceError);

    customers.remove("+79035550202");
    EXPECT_EQ(customers.listCustomers().size(), 1u);
    EXPECT_THROW(customers.getByPhone("+79035550202"), NotFoundError);
}

TEST_F(ServicesTest, CustomerWithOrdersCannotBeRemoved) {
    orders.receive("WB-1", "+79161234567", ParcelSize::Small);
    EXPECT_THROW(customers.remove("+79161234567"), ServiceError);
}

TEST_F(ServicesTest, ReceivePicksSmallestFittingFreeCell) {
    const OrderDetails first = orders.receive("WB-1", "+79161234567", ParcelSize::Small);
    EXPECT_EQ(first.cellCode, "A-01");
    EXPECT_TRUE(Order::isValidPickupCode(first.order.pickupCode()));
    EXPECT_EQ(first.storageDeadline, clock.now() + kStorage);

    // A-01 занята — маленькая посылка уходит в ячейку побольше.
    EXPECT_EQ(orders.receive("WB-2", "+79161234567", ParcelSize::Small).cellCode, "B-01");
    EXPECT_EQ(orders.receive("WB-3", "+79161234567", ParcelSize::Medium).cellCode, "C-01");
    EXPECT_THROW(orders.receive("WB-4", "+79161234567", ParcelSize::Small), ServiceError);
}

TEST_F(ServicesTest, ReceiveValidatesInput) {
    EXPECT_THROW(orders.receive("WB-1", "+79990000000", ParcelSize::Small), NotFoundError);
    EXPECT_THROW(orders.receive("??", "+79161234567", ParcelSize::Small), DomainError);
    orders.receive("WB-1", "+79161234567", ParcelSize::Small);
    EXPECT_THROW(orders.receive("wb-1", "+79161234567", ParcelSize::Small), ServiceError);
}

TEST_F(ServicesTest, IssueFreesCell) {
    const OrderDetails received = orders.receive("WB-1", "+79161234567", ParcelSize::Large);
    EXPECT_THROW(orders.receive("WB-2", "+79161234567", ParcelSize::Large), ServiceError);

    EXPECT_THROW(orders.issue("WB-1", "0000"), DomainError);
    clock.advance(2h);
    const OrderDetails issued = orders.issue("WB-1", received.order.pickupCode());
    EXPECT_EQ(issued.order.status(), OrderStatus::Issued);
    EXPECT_EQ(issued.order.closedAt(), clock.now());

    EXPECT_EQ(orders.receive("WB-2", "+79161234567", ParcelSize::Large).cellCode, "C-01");
}

TEST_F(ServicesTest, ReturnOverdueOrdersReturnsOnlyExpired) {
    orders.receive("OLD-1", "+79161234567", ParcelSize::Small);
    clock.advance(std::chrono::duration_cast<std::chrono::seconds>(kStorage) - 1h);
    orders.receive("NEW-1", "+79161234567", ParcelSize::Small);

    EXPECT_EQ(orders.returnOverdueOrders(), 0u);
    clock.advance(1h);
    EXPECT_EQ(orders.returnOverdueOrders(), 1u);
    EXPECT_EQ(orders.getByTrackingCode("OLD-1").order.status(), OrderStatus::Returned);
    EXPECT_EQ(orders.getByTrackingCode("NEW-1").order.status(), OrderStatus::Stored);
    EXPECT_EQ(orders.returnOverdueOrders(), 0u);  // повторный запуск ничего не меняет
}

TEST_F(ServicesTest, OrderClosedByAnotherServiceInstanceCannotBeIssued) {
    const OrderDetails received = orders.receive("WB-1", "+79161234567", ParcelSize::Small);

    // Два экземпляра сервиса (как у меню и фонового потока) видят одни и те же данные.
    OrderService other(orderRepo, customerRepo, cellRepo, clock, kStorage);
    other.returnToSender("WB-1");

    EXPECT_THROW(orders.issue("WB-1", received.order.pickupCode()), DomainError);
    EXPECT_EQ(orders.getByTrackingCode("WB-1").order.status(), OrderStatus::Returned);
}

TEST_F(ServicesTest, OccupiedCellCannotBeRemoved) {
    orders.receive("WB-1", "+79161234567", ParcelSize::Small);
    EXPECT_THROW(cells.removeCell("a-01"), ServiceError);
    EXPECT_NO_THROW(cells.removeCell("C-01"));
    EXPECT_THROW(cells.removeCell("C-01"), NotFoundError);

    const auto list = cells.listCells();
    ASSERT_EQ(list.size(), 2u);
    EXPECT_EQ(list[0].storedTrackingCode, "WB-1");
    EXPECT_TRUE(list[1].isFree());
}

TEST_F(ServicesTest, CustomerOrdersAreListedWithDetails) {
    orders.receive("WB-1", "+79161234567", ParcelSize::Small);
    orders.receive("WB-2", "+79161234567", ParcelSize::Medium);
    const auto list = orders.ordersOfCustomer("89161234567");
    ASSERT_EQ(list.size(), 2u);
    EXPECT_EQ(list[0].customerName, "Иванов Иван");
    EXPECT_EQ(orders.storedOrders().size(), 2u);
}
