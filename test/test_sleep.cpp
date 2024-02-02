#include <asyncpp/join.hpp>
#include <asyncpp/sleep.hpp>
#include <asyncpp/task.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace asyncpp;
using namespace std::chrono_literals;


TEST_CASE("Sleep: ordering", "[Sleep]") {
    std::vector<int> sequence;
    const auto delayed = [&sequence](int number, std::chrono::steady_clock::time_point time) -> task<void> {
        co_await sleep_until(time);
        sequence.push_back(number);
    };

    const auto time = std::chrono::steady_clock::now();
    auto t3 = launch(delayed(3, time + 3ms));
    auto t1 = launch(delayed(1, time + 1ms));
    auto t2 = launch(delayed(2, time + 2ms));
    join(t1);
    join(t2);
    join(t3);
    REQUIRE(sequence == std::vector{ 1, 2, 3 });
}


TEST_CASE("Sleep: sleep_for minimum timing", "[Sleep]") {
    const auto duration = 10ms;
    const auto start = std::chrono::steady_clock::now();
    join(sleep_for(duration));
    const auto end = std::chrono::steady_clock::now();
    REQUIRE(end - start >= duration);
}


TEST_CASE("Sleep: sleep_until minimum timing", "[Sleep]") {
    const auto duration = 10ms;
    const auto start = std::chrono::steady_clock::now();
    join(sleep_until(start + duration));
    const auto end = std::chrono::steady_clock::now();
    REQUIRE(end - start >= duration);
}
