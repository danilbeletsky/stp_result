#include "service/PeriodicWorker.h"

#include <exception>
#include <stdexcept>
#include <utility>

namespace pvz {

PeriodicWorker::PeriodicWorker(Task task, std::chrono::milliseconds interval)
    : task_(std::move(task)), interval_(interval) {
    if (!task_) {
        throw std::invalid_argument("PeriodicWorker: пустая задача");
    }
    if (interval_.count() <= 0) {
        throw std::invalid_argument("PeriodicWorker: интервал должен быть положительным");
    }
}

PeriodicWorker::~PeriodicWorker() {
    stop();
}

void PeriodicWorker::start() {
    std::lock_guard lock(mutex_);
    if (thread_.joinable()) {
        return;  // уже запущен
    }
    stopRequested_ = false;
    status_.running = true;
    thread_ = std::thread(&PeriodicWorker::loop, this);
}

void PeriodicWorker::stop() {
    {
        std::lock_guard lock(mutex_);
        if (!thread_.joinable()) {
            return;
        }
        stopRequested_ = true;
    }
    wakeUp_.notify_all();
    thread_.join();

    std::lock_guard lock(mutex_);
    status_.running = false;
}

PeriodicWorker::Status PeriodicWorker::status() const {
    std::lock_guard lock(mutex_);
    return status_;
}

void PeriodicWorker::loop() {
    while (true) {
        runOnce();

        std::unique_lock lock(mutex_);
        // wait_for вернёт true сразу, как только stop() выставит флаг.
        if (wakeUp_.wait_for(lock, interval_, [this] { return stopRequested_; })) {
            return;
        }
    }
}

void PeriodicWorker::runOnce() {
    // Задача выполняется без мьютекса: она может работать долго (запросы к БД),
    // а status() из основного потока не должен её ждать.
    std::size_t processed = 0;
    std::string error;
    try {
        processed = task_();
    } catch (const std::exception& e) {
        error = e.what();
    } catch (...) {
        error = "неизвестная ошибка";
    }

    std::lock_guard lock(mutex_);
    ++status_.runs;
    status_.lastRunAt = std::chrono::system_clock::now();
    status_.lastError = error;
    if (error.empty()) {
        status_.lastProcessed = processed;
        status_.totalProcessed += processed;
    }
}

} // namespace pvz
