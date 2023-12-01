#include "helper_interleaving.hpp"

#include <async++/interleaving/runner.hpp>
#include <async++/join.hpp>
#include <async++/task.hpp>

#include <catch2/catch_test_macros.hpp>


using namespace asyncpp;


TEST_CASE("Task: interleaving co_await", "[Task]") {
    static const auto sub_task = []() -> task<int> {
        co_return 3;
    };
    static const auto main_task = [](task<int> tsk) -> task<int> {
        tsk.launch();
        co_return co_await tsk;
    };

    auto interleavings = run_dependent_tasks(main_task, sub_task, std::tuple{}, std::tuple{});
    evaluate_interleavings(std::move(interleavings));
}


TEST_CASE("Task: interleaving abandon", "[Task]") {
    static const auto abandoned_task = []() -> task<int> { co_return 3; };

    auto interleavings = run_abandoned_task(abandoned_task, std::tuple{});
    evaluate_interleavings(std::move(interleavings));
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