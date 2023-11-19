#include "test_schedulers.hpp"

#include <async++/join.hpp>
#include <async++/task.hpp>
#include <async++/thread_pool.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>

#include <catch2/catch_test_macros.hpp>


using namespace asyncpp;


constexpr size_t num_threads = 4;
constexpr int64_t depth = 5;
constexpr size_t branching = 10;


TEST_CASE("Thread pool: perf test", "[Scheduler]") {
    thread_pool sched(num_threads);

    static const auto coro = [&sched](auto self, int depth) -> task<int64_t> {
        if (depth <= 0) {
            co_return 1;
        }
        std::array<task<int64_t>, branching> children;
        std::ranges::generate(children, [&] { return launch(self(self, depth - 1), sched); });
        int64_t sum = 0;
        for (auto& tk : children) {
            sum += co_await tk;
        }
        co_return sum;
    };

    const auto count = int64_t(std::pow(branching, depth));
    const auto start = std::chrono::high_resolution_clock::now();
    const auto result = join(bind(coro(coro, depth), sched));
    const auto end = std::chrono::high_resolution_clock::now();
    std::cout << "performance: " << 1e9 * double(count) / std::chrono::nanoseconds(end - start).count() << " / s" << std::endl;
    REQUIRE(result == count);
}