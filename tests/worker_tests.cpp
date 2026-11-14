#include "service/PeriodicWorker.h"

#include <gtest/gtest.h>

#include <atomic>
#include <stdexcept>
#include <thread>

using namespace pvz;
using namespace std::chrono_literals;

namespace {

// Ждёт условия не дольше timeout, чтобы тест не зависал при ошибке.
template <typename Predicate>
bool waitUntil(Predicate predicate, std::chrono::milliseconds timeout = 2s) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(5ms);
    }
    return predicate();
}

} // namespace

TEST(PeriodicWorkerTest, RunsTaskRepeatedlyAndCollectsStatus) {
    std::atomic<int> calls{0};
    PeriodicWorker worker([&calls] { ++calls; return std::size_t{2}; }, 10ms);
    worker.start();

    ASSERT_TRUE(waitUntil([&] { return worker.status().runs >= 3; }));
    worker.stop();

    const auto status = worker.status();
    EXPECT_FALSE(status.running);
    EXPECT_EQ(status.runs, static_cast<std::size_t>(calls.load()));
    EXPECT_EQ(status.totalProcessed, status.runs * 2);
    EXPECT_TRUE(status.lastError.empty());
}

TEST(PeriodicWorkerTest, StopDoesNotWaitForFullInterval) {
    PeriodicWorker worker([] { return std::size_t{0}; }, 1h);
    worker.start();
    ASSERT_TRUE(waitUntil([&] { return worker.status().runs == 1; }));

    const auto before = std::chrono::steady_clock::now();
    worker.stop();
    EXPECT_LT(std::chrono::steady_clock::now() - before, 1s);
    EXPECT_EQ(worker.status().runs, 1u);
}

TEST(PeriodicWorkerTest, TaskErrorIsReportedAndWorkerKeepsRunning) {
    std::atomic<int> calls{0};
    PeriodicWorker worker(
        [&calls]() -> std::size_t {
            if (++calls == 1) {
                throw std::runtime_error("нет связи с БД");
            }
            return 1;
        },
        10ms);
    worker.start();
    ASSERT_TRUE(waitUntil([&] { return worker.status().runs >= 2; }));
    worker.stop();

    const auto status = worker.status();
    EXPECT_TRUE(status.lastError.empty());  // вторая попытка прошла успешно
    EXPECT_EQ(status.totalProcessed, status.runs - 1);
}

TEST(PeriodicWorkerTest, DestructorStopsThread) {
    std::atomic<int> calls{0};
    {
        PeriodicWorker worker([&calls] { ++calls; return std::size_t{0}; }, 5ms);
        worker.start();
        ASSERT_TRUE(waitUntil([&] { return calls.load() > 0; }));
    }  // деструктор должен остановить поток и дождаться его
    const int afterDestroy = calls.load();
    std::this_thread::sleep_for(30ms);
    EXPECT_EQ(calls.load(), afterDestroy);
}

TEST(PeriodicWorkerTest, RejectsInvalidArguments) {
    EXPECT_THROW(PeriodicWorker(nullptr, 1s), std::invalid_argument);
    EXPECT_THROW(PeriodicWorker([] { return std::size_t{0}; }, 0ms), std::invalid_argument);
}
