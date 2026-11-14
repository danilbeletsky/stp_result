#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace pvz {

// Запускает задачу в отдельном потоке: сразу после start() и затем раз в interval.
// Задача возвращает количество обработанных элементов (например, возвращённых заказов).
//
// Остановка: stop() выставляет флаг под мьютексом, будит поток через condition_variable
// и делает join(). Поэтому выход не ждёт окончания интервала. Деструктор вызывает stop().
class PeriodicWorker {
public:
    using Task = std::function<std::size_t()>;

    struct Status {
        bool running = false;
        std::size_t runs = 0;            // сколько раз задача выполнилась (успешно или нет)
        std::size_t totalProcessed = 0;  // сумма результатов задачи
        std::size_t lastProcessed = 0;
        std::optional<std::chrono::system_clock::time_point> lastRunAt;
        std::string lastError;           // пусто, если последний запуск прошёл без ошибок
    };

    PeriodicWorker(Task task, std::chrono::milliseconds interval);
    ~PeriodicWorker();

    PeriodicWorker(const PeriodicWorker&) = delete;
    PeriodicWorker& operator=(const PeriodicWorker&) = delete;

    void start();
    void stop();

    Status status() const;

private:
    void loop();
    void runOnce();

    Task task_;
    std::chrono::milliseconds interval_;

    mutable std::mutex mutex_;  // защищает stopRequested_ и status_
    std::condition_variable wakeUp_;
    bool stopRequested_ = false;
    Status status_;

    std::thread thread_;
};

} // namespace pvz
