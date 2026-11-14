#include "cli/App.h"

#include "cli/Format.h"
#include "domain/DomainError.h"
#include "repository/StorageError.h"
#include "service/ServiceError.h"

#include <ostream>
#include <utility>

namespace pvz::cli {

App::App(Console& console, CustomerService& customers, CellService& cells, OrderService& orders,
         const PeriodicWorker& overdueWorker, std::string storageDescription)
    : console_(console),
      customers_(customers),
      cells_(cells),
      orders_(orders),
      overdueWorker_(overdueWorker),
      storageDescription_(std::move(storageDescription)) {}

template <typename Action>
void App::guarded(Action&& action) {
    std::ostream& out = console_.out();
    try {
        action();
    } catch (const OperationCancelled&) {
        out << "Операция отменена.\n";
    } catch (const DomainError& error) {
        out << "Ошибка: " << error.what() << "\n";
    } catch (const ServiceError& error) {
        out << "Ошибка: " << error.what() << "\n";
    } catch (const ConnectionError& error) {
        out << "Нет связи с базой данных: " << error.what()
            << "\nПроверьте, что PostgreSQL запущен; при следующей операции будет новая попытка.\n";
    } catch (const StorageError& error) {
        out << "Ошибка базы данных: " << error.what() << "\n";
    }
    // InputClosed и прочие исключения пробрасываются выше.
}

void App::run() {
    std::ostream& out = console_.out();
    out << "Пункт выдачи заказов. База данных: " << storageDescription_
        << ". Срок хранения: " << orders_.storagePeriod().count() << " дн.\n";

    while (true) {
        printMainMenu();
        const int choice = console_.askNumber("Выберите пункт: ", 0, 10);
        out << "\n";
        switch (choice) {
        case 1: guarded([this] { receiveOrder(); }); break;
        case 2: guarded([this] { issueOrder(); }); break;
        case 3: guarded([this] { returnOrder(); }); break;
        case 4: guarded([this] { findOrder(); }); break;
        case 5: guarded([this] { showCustomerOrders(); }); break;
        case 6: guarded([this] { showStoredOrders(); }); break;
        case 7: guarded([this] { returnOverdueNow(); }); break;
        case 8: showWorkerStatus(); break;
        case 9: customersMenu(); break;
        case 10: cellsMenu(); break;
        case 0: out << "До свидания!\n"; return;
        }
        out << "\n";
    }
}

void App::printMainMenu() {
    console_.out() << "==== Главное меню ====\n"
                      " 1. Принять заказ\n"
                      " 2. Выдать заказ\n"
                      " 3. Вернуть заказ отправителю (отказ)\n"
                      " 4. Найти заказ по трек-номеру\n"
                      " 5. Заказы клиента\n"
                      " 6. Заказы на хранении\n"
                      " 7. Вернуть просроченные заказы сейчас\n"
                      " 8. Состояние фоновой проверки\n"
                      " 9. Клиенты…\n"
                      "10. Ячейки…\n"
                      " 0. Выход\n";
}

std::string App::askPhone() {
    return console_.askParsed<std::string>("Телефон клиента: ", [](const std::string& line) {
        return Customer::normalizePhone(line);
    });
}

std::string App::askTrackingCode() {
    return console_.askParsed<std::string>("Трек-номер: ", [](const std::string& line) {
        return Order::normalizeTrackingCode(line);
    });
}

// ---------------------------------------------------------------- заказы

void App::receiveOrder() {
    std::ostream& out = console_.out();
    const std::string tracking = askTrackingCode();
    const std::string phone = askPhone();

    if (!customers_.findByPhone(phone)) {
        out << "Клиент " << phone << " ещё не зарегистрирован.\n";
        if (!console_.confirm("Зарегистрировать нового клиента?")) {
            throw OperationCancelled();
        }
        const std::string name = console_.askParsed<std::string>(
            "ФИО клиента: ", [](const std::string& line) { return Customer::normalizeName(line); });
        const Customer customer = customers_.registerCustomer(name, phone);
        out << "Клиент " << customer.fullName() << " зарегистрирован.\n";
    }

    const ParcelSize size = console_.askParsed<ParcelSize>(
        "Размер посылки (S/M/L): ", [](const std::string& line) { return parseParcelSize(line); });

    const OrderDetails details = orders_.receive(tracking, phone, size);
    out << "Заказ принят.\n"
        << "  Положите посылку в ячейку: " << details.cellCode << "\n"
        << "  Код получения для SMS клиенту: " << details.order.pickupCode() << "\n"
        << "  Хранится до: " << formatDate(details.storageDeadline) << "\n";
}

void App::issueOrder() {
    const std::string tracking = askTrackingCode();
    const std::string code = console_.askParsed<std::string>(
        "Код получения: ", [](const std::string& line) {
            if (!Order::isValidPickupCode(line)) {
                throw DomainError("Код получения — 4 цифры");
            }
            return line;
        });
    const OrderDetails details = orders_.issue(tracking, code);
    console_.out() << "Заберите посылку из ячейки " << details.cellCode << " и отдайте клиенту "
                   << details.customerName << ".\n";
}

void App::returnOrder() {
    const std::string tracking = askTrackingCode();
    const OrderDetails details = orders_.getByTrackingCode(tracking);
    printOrderCard(details);
    if (!console_.confirm("Вернуть этот заказ отправителю?")) {
        throw OperationCancelled();
    }
    const OrderDetails returned = orders_.returnToSender(tracking);
    console_.out() << "Заказ " << returned.order.trackingCode()
                   << " возвращён отправителю, ячейка освобождена.\n";
}

void App::findOrder() {
    printOrderCard(orders_.getByTrackingCode(askTrackingCode()));
}

void App::showCustomerOrders() {
    const std::string phone = askPhone();
    const auto orders = orders_.ordersOfCustomer(phone);
    if (orders.empty()) {
        console_.out() << "У клиента нет заказов.\n";
        return;
    }
    printOrderTable(orders);
}

void App::showStoredOrders() {
    const auto orders = orders_.storedOrders();
    if (orders.empty()) {
        console_.out() << "На хранении ничего нет.\n";
        return;
    }
    printOrderTable(orders);
}

void App::returnOverdueNow() {
    const std::size_t returned = orders_.returnOverdueOrders();
    console_.out() << "Возвращено просроченных заказов: " << returned << "\n";
}

void App::showWorkerStatus() {
    const PeriodicWorker::Status status = overdueWorker_.status();
    std::ostream& out = console_.out();
    out << "Фоновая проверка просроченных заказов: " << (status.running ? "работает" : "остановлена")
        << "\n  Запусков: " << status.runs
        << "\n  Возвращено всего: " << status.totalProcessed;
    if (status.lastRunAt) {
        out << "\n  Последний запуск: " << formatDateTime(*status.lastRunAt)
            << " (возвращено: " << status.lastProcessed << ")";
    }
    if (!status.lastError.empty()) {
        out << "\n  Ошибка последнего запуска: " << status.lastError;
    }
    out << "\n";
}

void App::printOrderCard(const OrderDetails& details) {
    const Order& order = details.order;
    std::ostream& out = console_.out();
    out << "Заказ " << order.trackingCode() << "\n"
        << "  Статус:  " << toDisplayName(order.status()) << "\n"
        << "  Клиент:  " << details.customerName << ", " << details.customerPhone << "\n"
        << "  Размер:  " << toDisplayName(order.parcelSize()) << "\n"
        << "  Ячейка:  " << details.cellCode << "\n"
        << "  Принят:  " << formatDateTime(order.receivedAt()) << "\n";
    if (order.isStored()) {
        out << "  Хранить до: " << formatDate(details.storageDeadline) << "\n";
    } else if (order.closedAt()) {
        out << "  Закрыт:  " << formatDateTime(*order.closedAt()) << "\n";
    }
}

void App::printOrderTable(const std::vector<OrderDetails>& orders) {
    std::ostream& out = console_.out();
    out << pad("Трек-номер", 16) << pad("Статус", 23) << pad("Ячейка", 8) << pad("Принят", 18)
        << pad("Срок / закрыт", 18) << "Клиент\n";
    for (const OrderDetails& details : orders) {
        const Order& order = details.order;
        const std::string lastDate = order.isStored()
                                         ? "до " + formatDate(details.storageDeadline)
                                         : formatDateTime(order.closedAt().value_or(order.receivedAt()));
        out << pad(order.trackingCode(), 16) << pad(toDisplayName(order.status()), 23)
            << pad(details.cellCode, 8) << pad(formatDateTime(order.receivedAt()), 18)
            << pad(lastDate, 18) << details.customerName << " " << details.customerPhone << "\n";
    }
    out << "Всего: " << orders.size() << "\n";
}

// ---------------------------------------------------------------- клиенты

void App::customersMenu() {
    while (true) {
        console_.out() << "---- Клиенты ----\n"
                          " 1. Список клиентов\n"
                          " 2. Добавить клиента\n"
                          " 3. Изменить ФИО\n"
                          " 4. Изменить телефон\n"
                          " 5. Удалить клиента\n"
                          " 0. Назад\n";
        const int choice = console_.askNumber("Выберите пункт: ", 0, 5);
        console_.out() << "\n";
        switch (choice) {
        case 1: guarded([this] { listCustomers(); }); break;
        case 2: guarded([this] { addCustomer(); }); break;
        case 3: guarded([this] { renameCustomer(); }); break;
        case 4: guarded([this] { changeCustomerPhone(); }); break;
        case 5: guarded([this] { removeCustomer(); }); break;
        case 0: return;
        }
        console_.out() << "\n";
    }
}

void App::listCustomers() {
    const auto customers = customers_.listCustomers();
    if (customers.empty()) {
        console_.out() << "Клиентов пока нет.\n";
        return;
    }
    console_.out() << pad("Телефон", 14) << "ФИО\n";
    for (const Customer& customer : customers) {
        console_.out() << pad(customer.phone(), 14) << customer.fullName() << "\n";
    }
    console_.out() << "Всего: " << customers.size() << "\n";
}

void App::addCustomer() {
    const std::string name = console_.askParsed<std::string>(
        "ФИО: ", [](const std::string& line) { return Customer::normalizeName(line); });
    const std::string phone = askPhone();
    const Customer customer = customers_.registerCustomer(name, phone);
    console_.out() << "Клиент " << customer.fullName() << " (" << customer.phone()
                   << ") зарегистрирован.\n";
}

void App::renameCustomer() {
    const std::string phone = askPhone();
    const Customer current = customers_.getByPhone(phone);
    console_.out() << "Сейчас: " << current.fullName() << "\n";
    const std::string newName = console_.askParsed<std::string>(
        "Новое ФИО: ", [](const std::string& line) { return Customer::normalizeName(line); });
    const Customer updated = customers_.rename(phone, newName);
    console_.out() << "Сохранено: " << updated.fullName() << "\n";
}

void App::changeCustomerPhone() {
    const std::string phone = askPhone();
    const Customer current = customers_.getByPhone(phone);
    console_.out() << "Клиент: " << current.fullName() << "\n";
    const std::string newPhone = console_.askParsed<std::string>(
        "Новый телефон: ", [](const std::string& line) { return Customer::normalizePhone(line); });
    const Customer updated = customers_.changePhone(phone, newPhone);
    console_.out() << "Новый телефон: " << updated.phone() << "\n";
}

void App::removeCustomer() {
    const std::string phone = askPhone();
    const Customer customer = customers_.getByPhone(phone);
    if (!console_.confirm("Удалить клиента " + customer.fullName() + "?")) {
        throw OperationCancelled();
    }
    customers_.remove(phone);
    console_.out() << "Клиент удалён.\n";
}

// ---------------------------------------------------------------- ячейки

void App::cellsMenu() {
    while (true) {
        console_.out() << "---- Ячейки ----\n"
                          " 1. Список ячеек\n"
                          " 2. Добавить ячейку\n"
                          " 3. Удалить пустую ячейку\n"
                          " 0. Назад\n";
        const int choice = console_.askNumber("Выберите пункт: ", 0, 3);
        console_.out() << "\n";
        switch (choice) {
        case 1: guarded([this] { listCells(); }); break;
        case 2: guarded([this] { addCell(); }); break;
        case 3: guarded([this] { removeCell(); }); break;
        case 0: return;
        }
        console_.out() << "\n";
    }
}

void App::listCells() {
    const auto cells = cells_.listCells();
    if (cells.empty()) {
        console_.out() << "Ячеек нет. Добавьте их в меню «Ячейки».\n";
        return;
    }
    std::size_t freeCount = 0;
    console_.out() << pad("Ячейка", 8) << pad("Размер", 14) << "Содержимое\n";
    for (const CellView& view : cells) {
        if (view.isFree()) {
            ++freeCount;
        }
        console_.out() << pad(view.cell.code(), 8) << pad(toDisplayName(view.cell.size()), 14)
                       << view.storedTrackingCode.value_or("свободна") << "\n";
    }
    console_.out() << "Всего: " << cells.size() << ", свободно: " << freeCount << "\n";
}

void App::addCell() {
    const std::string code = console_.askParsed<std::string>(
        "Код ячейки (например A-01): ",
        [](const std::string& line) { return Cell::normalizeCode(line); });
    const ParcelSize size = console_.askParsed<ParcelSize>(
        "Размер ячейки (S/M/L): ", [](const std::string& line) { return parseParcelSize(line); });
    const Cell cell = cells_.addCell(code, size);
    console_.out() << "Ячейка " << cell.code() << " (" << toDisplayName(cell.size())
                   << ") добавлена.\n";
}

void App::removeCell() {
    const std::string code = console_.askParsed<std::string>(
        "Код ячейки: ", [](const std::string& line) { return Cell::normalizeCode(line); });
    cells_.removeCell(code);
    console_.out() << "Ячейка " << code << " удалена.\n";
}

} // namespace pvz::cli
