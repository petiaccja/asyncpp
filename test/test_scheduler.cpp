#include <async++/eager_task.hpp>
#include <async++/scheduler.hpp>
#include <async++/task.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>

#include <catch2/catch_test_macros.hpp>


using namespace asyncpp;


constexpr size_t num_threads = 8;
constexpr int64_t depth = 8;


TEST_CASE("Scheduler: thread pool", "[Scheduler]") {
    thread_pool sched(num_threads);

    static const auto coro = [&sched](auto self, int depth) -> task<int64_t> {
        if (depth <= 0) {
            co_return 1;
        }
        std::array<task<int64_t>, 10> children;
        std::ranges::generate(children, [&] { return set_scheduler(self(self, depth - 1), sched); });
        int64_t sum = 0;
        for (auto& tk : children) {
            sum += co_await tk;
        }
        co_return sum;
    };

    const auto count = int64_t(std::pow(10, depth));
    const auto start = std::chrono::high_resolution_clock::now();
    const auto result = set_scheduler(coro(coro, depth), sched).get();
    const auto end = std::chrono::high_resolution_clock::now();
    std::cout << "performance: " << 1e9 * double(count) / std::chrono::nanoseconds(end - start).count() << " / s";
    REQUIRE(result == count);
}


TEST_CASE("Scheduler: thread pool eager", "[Scheduler]") {
    thread_pool sched(num_threads);

    static const auto coro = [&sched](auto self, int depth) -> eager_task<int64_t> {
        if (depth <= 0) {
            co_return 1;
        }
        std::array<eager_task<int64_t>, 10> children;
        std::ranges::generate(children, [&] { return set_scheduler(self(self, depth - 1), sched); });
        int64_t sum = 0;
        for (auto& tk : children) {
            tk.start();
        }
        for (auto& tk : children) {
            sum += co_await tk;
        }
        co_return sum;
    };

    const auto count = int64_t(std::pow(10, depth));
    const auto start = std::chrono::high_resolution_clock::now();
    const auto result = set_scheduler(coro(coro, depth), sched).get();
    const auto end = std::chrono::high_resolution_clock::now();
    std::cout << "performance: " << 1e9 * double(count) / std::chrono::nanoseconds(end - start).count() << " / s" << std::endl;
    REQUIRE(result == count);
}