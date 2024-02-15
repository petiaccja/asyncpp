#include "helper_interleaving.hpp"

#include <asyncpp/join.hpp>
#include <asyncpp/task.hpp>
#include <asyncpp/testing/interleaver.hpp>

#include <catch2/catch_test_macros.hpp>


using namespace asyncpp;


TEST_CASE("Task: interleaving co_await", "[Task]") {
    INTERLEAVED_RUN(
        await_task_scenario<task>,
        THREAD("awaited", &await_task_scenario<task>::awaited),
        THREAD("awaiter", &await_task_scenario<task>::awaiter));
}


TEST_CASE("Task: interleaving abandon", "[Task]") {
    INTERLEAVED_RUN(
        abandon_task_scenario<task>,
        THREAD("task", &abandon_task_scenario<task>::task),
        THREAD("abandon", &abandon_task_scenario<task>::abandon));
}


TEST_CASE("Task: abandon (not started)", "[Shared task]") {
    static const auto coro = []() -> task<void> {
        co_return;
    };
    const auto before = impl::leak_checked_promise::snapshot();
    static_cast<void>(coro());
    REQUIRE(impl::leak_checked_promise::check(before));
}


TEST_CASE("Task: co_await value", "[Task]") {
    static const auto coro = [](int value) -> task<int> {
        co_return value;
    };
    static const auto enclosing = [](int value) -> task<int> {
        co_return co_await coro(value);
    };
    REQUIRE(join(enclosing(42)) == 42);
}


TEST_CASE("Task: co_await ref", "[Task]") {
    static int value = 42;
    static const auto coro = [](int& value) -> task<int&> {
        co_return value;
    };
    static const auto enclosing = [](int& value) -> task<int&> {
        co_return co_await coro(value);
    };
    auto task = enclosing(value);
    auto& result = join(task);
    REQUIRE(result == 42);
    REQUIRE(&result == &value);
}


TEST_CASE("Task: co_await void", "[Task]") {
    static int value = 42;
    static const auto coro = []() -> task<void> {
        co_return;
    };
    static const auto enclosing = []() -> task<void> {
        co_await coro();
    };
    auto task = enclosing();
    join(task);
}