#include "helper_interleaving.hpp"

#include <async++/interleaving/runner.hpp>
#include <async++/join.hpp>
#include <async++/shared_task.hpp>

#include <catch2/catch_test_macros.hpp>


using namespace asyncpp;


TEST_CASE("Shared task: interleaving co_await", "[Shared task]") {
    static const auto sub_task = [](leak_tester tester) -> shared_task<int> {
        co_return 3;
    };
    static const auto main_task = [](shared_task<int> tsk) -> shared_task<int> {
        tsk.launch();
        co_return co_await tsk;
    };

    leak_tester tester;
    auto interleavings = run_dependent_tasks(main_task, sub_task, std::tuple{}, std::tuple{ tester });
    evaluate_interleavings(std::move(interleavings), std::move(tester));
}


TEST_CASE("Shared task: interleaving abandon", "[Shared task]") {
    static const auto abandoned_task = [](leak_tester value) -> shared_task<int> { co_return 3; };

    leak_tester tester;
    auto interleavings = run_abandoned_task(abandoned_task, std::tuple{ tester });
    evaluate_interleavings(std::move(interleavings), std::move(tester));
}


TEST_CASE("Shared task: abandon (not started)", "[Shared task]") {
    static const auto coro = [](leak_tester tester) -> shared_task<void> {
        co_return;
    };
    leak_tester tester;
    static_cast<void>(coro(tester));
    REQUIRE(tester);
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