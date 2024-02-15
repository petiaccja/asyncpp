#include "helper_interleaving.hpp"

#include <asyncpp/join.hpp>
#include <asyncpp/shared_task.hpp>
#include <asyncpp/testing/interleaver.hpp>

#include <catch2/catch_test_macros.hpp>


using namespace asyncpp;


TEST_CASE("Shared task: interleaving co_await", "[Shared task]") {
    INTERLEAVED_RUN(
        await_task_scenario<shared_task>,
        THREAD("awaited", &await_task_scenario<shared_task>::awaited),
        THREAD("awaiter", &await_task_scenario<shared_task>::awaiter));
}


TEST_CASE("Shared task: interleaving abandon", "[Shared task]") {
    INTERLEAVED_RUN(
        abandon_task_scenario<shared_task>,
        THREAD("task", &abandon_task_scenario<shared_task>::task),
        THREAD("abandon", &abandon_task_scenario<shared_task>::abandon));
}


TEST_CASE("Shared task: abandon (not started)", "[Shared task]") {
    static const auto coro = []() -> shared_task<void> {
        co_return;
    };
    const auto before = impl::leak_checked_promise::snapshot();
    static_cast<void>(coro());
    REQUIRE(impl::leak_checked_promise::check(before));
}


TEST_CASE("Shared task: co_await value", "[Shared task]") {
    static const auto coro = [](int value) -> shared_task<int> {
        co_return value;
    };
    static const auto enclosing = [](int value) -> shared_task<int> {
        co_return co_await coro(value);
    };
    REQUIRE(join(enclosing(42)) == 42);
}


TEST_CASE("Shared task: co_await ref", "[Shared task]") {
    static int value = 42;
    static const auto coro = [](int& value) -> shared_task<int&> {
        co_return value;
    };
    static const auto enclosing = [](int& value) -> shared_task<int&> {
        co_return co_await coro(value);
    };
    auto task = enclosing(value);
    auto& result = join(task);
    REQUIRE(result == 42);
    REQUIRE(&result == &value);
}


TEST_CASE("Shared task: co_await void", "[Shared task]") {
    static int value = 42;
    static const auto coro = []() -> shared_task<void> {
        co_return;
    };
    static const auto enclosing = []() -> shared_task<void> {
        co_await coro();
    };
    auto task = enclosing();
    join(task);
}