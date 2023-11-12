#include "test_schedulers.hpp"
#include <async++/async_task.hpp>
#include <async++/thread_pool.hpp>
#include <async++/task.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>

#include <catch2/catch_test_macros.hpp>


using namespace asyncpp;


constexpr size_t num_threads = 4;
constexpr int64_t depth = 6;
constexpr size_t branching = 10;


TEST_CASE("Thread pool: lazy task", "[Scheduler]") {
    thread_pool sched(num_threads);

    static const auto coro = [&sched](auto self, int depth) -> task<int64_t> {
        if (depth <= 0) {
            co_return 1;
        }
        std::array<task<int64_t>, branching> children;
        std::ranges::generate(children, [&] { return set_scheduler(self(self, depth - 1), sched); });
        int64_t sum = 0;
        for (auto& tk : children) {
            sum += co_await tk;
        }
        co_return sum;
    };

    const auto count = int64_t(std::pow(branching, depth));
    const auto start = std::chrono::high_resolution_clock::now();
    const auto result = set_scheduler(coro(coro, depth), sched).get();
    const auto end = std::chrono::high_resolution_clock::now();
    std::cout << "performance: " << 1e9 * double(count) / std::chrono::nanoseconds(end - start).count() << " / s";
    REQUIRE(result == count);
}


TEST_CASE("Thread pool: async task", "[Scheduler]") {
    thread_pool sched(num_threads);

    static const auto coro = [&sched](auto self, int depth) -> async_task<int64_t> {
        if (depth <= 0) {
            co_return 1;
        }
        std::array<async_task<int64_t>, branching> children;
        std::ranges::generate(children, [&] { return set_scheduler(self(self, depth - 1), sched); });
        int64_t sum = 0;
        for (auto& tk : children) {
            tk.launch();
        }
        for (auto& tk : children) {
            sum += co_await tk;
        }
        co_return sum;
    };

    const auto count = int64_t(std::pow(branching, depth));
    const auto start = std::chrono::high_resolution_clock::now();
    const auto result = set_scheduler(coro(coro, depth), sched).get();
    const auto end = std::chrono::high_resolution_clock::now();
    std::cout << "performance: " << 1e9 * double(count) / std::chrono::nanoseconds(end - start).count() << " / s" << std::endl;
    REQUIRE(result == count);
}