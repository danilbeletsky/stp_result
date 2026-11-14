#include "domain/Cell.h"
#include "domain/Customer.h"
#include "domain/DomainError.h"
#include "domain/Order.h"

#include <gtest/gtest.h>

using namespace pvz;
using namespace std::chrono_literals;

namespace {

const TimePoint kNow = TimePoint(std::chrono::seconds(1'700'000'000));
constexpr std::chrono::days kStorage{7};

std::string repeat(const std::string& piece, int times) {
    std::string result;
    for (int i = 0; i < times; ++i) {
        result += piece;
    }
    return result;
}

Order storedOrder() {
    return Order::receive("wb-123456", 1, 10, ParcelSize::Medium, "4321", kNow);
}

} // namespace

TEST(CustomerTest, NormalizesDifferentPhoneFormats) {
    EXPECT_EQ(Customer::normalizePhone("+7 (916) 123-45-67"), "+79161234567");
    EXPECT_EQ(Customer::normalizePhone("89161234567"), "+79161234567");
    EXPECT_EQ(Customer::normalizePhone("79161234567"), "+79161234567");
    EXPECT_EQ(Customer::normalizePhone("9161234567"), "+79161234567");
}

TEST(CustomerTest, RejectsInvalidPhones) {
    EXPECT_THROW(Customer::normalizePhone(""), DomainError);
    EXPECT_THROW(Customer::normalizePhone("12345"), DomainError);
    EXPECT_THROW(Customer::normalizePhone("+8 916 123 45 67"), DomainError);
    EXPECT_THROW(Customer::normalizePhone("8916abc4567"), DomainError);
    EXPECT_THROW(Customer::normalizePhone("+7916123456789"), DomainError);
}

TEST(CustomerTest, ValidatesNameAndChangesStateThroughMethods) {
    Customer customer = Customer::create("  Иванов Иван ", "89161234567");
    EXPECT_TRUE(customer.isNew());
    EXPECT_EQ(customer.fullName(), "Иванов Иван");

    customer.rename("Петров Пётр");
    EXPECT_EQ(customer.fullName(), "Петров Пётр");
    EXPECT_THROW(customer.rename("   "), DomainError);
    EXPECT_EQ(customer.fullName(), "Петров Пётр");  // неудачное изменение не портит объект

    // Лимит считается в символах: 100 букв «я» — это 200 байт, но имя допустимо.
    EXPECT_NO_THROW(customer.rename(repeat("я", 100)));
    EXPECT_THROW(customer.rename(repeat("я", 101)), DomainError);
}

TEST(CellTest, NormalizesCodeAndChecksSize) {
    const Cell cell = Cell::create(" b-02 ", ParcelSize::Medium);
    EXPECT_EQ(cell.code(), "B-02");
    EXPECT_TRUE(cell.canHold(ParcelSize::Small));
    EXPECT_TRUE(cell.canHold(ParcelSize::Medium));
    EXPECT_FALSE(cell.canHold(ParcelSize::Large));

    EXPECT_THROW(Cell::create("", ParcelSize::Small), DomainError);
    EXPECT_THROW(Cell::create("---", ParcelSize::Small), DomainError);
    EXPECT_THROW(Cell::create("A 01", ParcelSize::Small), DomainError);
    EXPECT_THROW(Cell::create("ABCDEFGHIJK", ParcelSize::Small), DomainError);
}

TEST(ParcelSizeTest, ParsesCodes) {
    EXPECT_EQ(parseParcelSize("s"), ParcelSize::Small);
    EXPECT_EQ(parseParcelSize(" M "), ParcelSize::Medium);
    EXPECT_EQ(parseParcelSize("L"), ParcelSize::Large);
    EXPECT_THROW(parseParcelSize("XL"), DomainError);
}

TEST(OrderTest, ReceivedOrderIsStoredInCell) {
    const Order order = storedOrder();
    EXPECT_EQ(order.trackingCode(), "WB-123456");
    EXPECT_TRUE(order.isStored());
    EXPECT_EQ(order.cellId(), Id{10});
    EXPECT_FALSE(order.closedAt().has_value());
    EXPECT_EQ(order.storageDeadline(kStorage), kNow + kStorage);
}

TEST(OrderTest, IssueRequiresCorrectCode) {
    Order order = storedOrder();
    EXPECT_THROW(order.issue("0000", kNow + 1h, kStorage), DomainError);
    EXPECT_TRUE(order.isStored());

    order.issue("4321", kNow + 1h, kStorage);
    EXPECT_EQ(order.status(), OrderStatus::Issued);
    EXPECT_EQ(order.closedAt(), kNow + 1h);
}

TEST(OrderTest, ClosedOrderCannotBeIssuedOrReturnedAgain) {
    Order order = storedOrder();
    order.issue("4321", kNow, kStorage);
    EXPECT_THROW(order.issue("4321", kNow, kStorage), DomainError);
    EXPECT_THROW(order.returnToSender(kNow), DomainError);

    Order returned = storedOrder();
    returned.returnToSender(kNow);
    EXPECT_EQ(returned.status(), OrderStatus::Returned);
    EXPECT_THROW(returned.issue("4321", kNow, kStorage), DomainError);
}

TEST(OrderTest, OverdueOrderCannotBeIssued) {
    Order order = storedOrder();
    EXPECT_FALSE(order.isOverdue(kNow + kStorage - 1s, kStorage));
    EXPECT_TRUE(order.isOverdue(kNow + kStorage, kStorage));
    EXPECT_THROW(order.issue("4321", kNow + kStorage, kStorage), DomainError);
}

TEST(OrderTest, RestoreRejectsInconsistentData) {
    OrderRecord record;
    record.id = 5;
    record.trackingCode = "TRACK-1";
    record.customerId = 1;
    record.cellId = 2;
    record.pickupCode = "1111";
    record.status = OrderStatus::Stored;
    record.receivedAt = kNow;
    EXPECT_NO_THROW(Order::restore(record));

    OrderRecord noCell = record;
    noCell.cellId.reset();
    EXPECT_THROW(Order::restore(noCell), DomainError);

    OrderRecord issuedWithoutDate = record;
    issuedWithoutDate.status = OrderStatus::Issued;
    EXPECT_THROW(Order::restore(issuedWithoutDate), DomainError);

    OrderRecord badCode = record;
    badCode.pickupCode = "12a4";
    EXPECT_THROW(Order::restore(badCode), DomainError);
}

TEST(OrderTest, ValidatesTrackingCode) {
    EXPECT_EQ(Order::normalizeTrackingCode(" oz-1 "), "OZ-1");
    EXPECT_THROW(Order::normalizeTrackingCode("abc"), DomainError);
    EXPECT_THROW(Order::normalizeTrackingCode("TRACK 1"), DomainError);
}
