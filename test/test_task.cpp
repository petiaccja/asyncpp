#include "leak_tester.hpp"
#include <async++/task.hpp>

#include <catch2/catch_test_macros.hpp>


using namespace asyncpp;


TEST_CASE("Task: destroy", "[Task]") {
    static const auto coro = [](leak_tester value) -> task<void> { co_return; };

    SECTION("no execution") {
        leak_tester tester;
        REQUIRE(tester); // Test the leak tester.
        {
            auto task = coro(tester);
            REQUIRE(!tester); // Test the leak tester.
        }
        REQUIRE(tester);
    }
    SECTION("synced") {
        leak_tester tester;
        {
            auto task = coro(tester);
            task.get();
        }
        REQUIRE(tester);
    }
}


TEST_CASE("Task: get value", "[Task]") {
    static const auto coro = [](int value) -> task<int> {
        co_return value;
    };
    REQUIRE(coro(42).get() == 42);
}


TEST_CASE("Task: co_await value", "[Task]") {
    static const auto coro = [](int value) -> task<int> {
        co_return value;
    };
    static const auto enclosing = [](int value) -> task<int> {
        co_return co_await coro(value);
    };
    REQUIRE(enclosing(42).get() == 42);
}


TEST_CASE("Task: get ref", "[Task]") {
    static const auto coro = [](int value) -> task<int> {
        co_return value;
    };
    REQUIRE(coro(42).get() == 42);
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
    auto& result = task.get();
    REQUIRE(result == 42);
    REQUIRE(&result == &value);
}