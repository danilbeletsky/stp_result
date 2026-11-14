#pragma once

#include "cli/Console.h"
#include "service/CellService.h"
#include "service/CustomerService.h"
#include "service/OrderService.h"
#include "service/PeriodicWorker.h"

#include <string>

namespace pvz::cli {

// Консольное меню оператора пункта выдачи.
// Работает только через сервисы: здесь нет SQL и нет обращений к базе.
class App {
public:
    App(Console& console, CustomerService& customers, CellService& cells, OrderService& orders,
        const PeriodicWorker& overdueWorker, std::string storageDescription);

    void run();

private:
    // Выполняет действие и превращает исключения в понятные сообщения,
    // чтобы ошибка одной операции не завершала программу.
    template <typename Action>
    void guarded(Action&& action);

    void printMainMenu();

    void receiveOrder();
    void issueOrder();
    void returnOrder();
    void findOrder();
    void showCustomerOrders();
    void showStoredOrders();
    void returnOverdueNow();
    void showWorkerStatus();

    void customersMenu();
    void listCustomers();
    void addCustomer();
    void renameCustomer();
    void changeCustomerPhone();
    void removeCustomer();

    void cellsMenu();
    void listCells();
    void addCell();
    void removeCell();

    std::string askPhone();
    std::string askTrackingCode();
    void printOrderCard(const OrderDetails& details);
    void printOrderTable(const std::vector<OrderDetails>& orders);

    Console& console_;
    CustomerService& customers_;
    CellService& cells_;
    OrderService& orders_;
    const PeriodicWorker& overdueWorker_;
    std::string storageDescription_;
};

} // namespace pvz::cli
