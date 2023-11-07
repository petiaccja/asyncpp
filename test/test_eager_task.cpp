#include "leak_tester.hpp"
#include <async++/eager_task.hpp>

#include <catch2/catch_test_macros.hpp>


using namespace asyncpp;


TEST_CASE("Eager task: destroy", "[Eager task]") {
    static const auto coro = [](leak_tester value) -> eager_task<void> { co_return; };
    static const auto awaiter = [](eager_task<void> tsk) -> eager_task<void> { co_return co_await tsk; };

    SECTION("no execution") {
        leak_tester tester;
        REQUIRE(tester); // Test the leak tester.
        {
            auto tsk = coro(tester);
            REQUIRE(!tester); // Test the leak tester.
        }
        REQUIRE(tester);
    }
    SECTION("abandoned") {
        leak_tester tester;
        {
            auto tsk = coro(tester);
            tsk.start();
        }
        REQUIRE(tester);
    }
    SECTION("synced") {
        leak_tester tester;
        {
            auto tsk = coro(tester);
            tsk.get();
        }
        REQUIRE(tester);
    }
    SECTION("co_awaited") {
        leak_tester tester;
        {
            auto tsk = coro(tester);
            auto encl = awaiter(std::move(tsk));
            encl.get();
        }
        REQUIRE(tester);
    }
    SECTION("eager synced") {
        leak_tester tester;
        {
            auto tsk = coro(tester);
            tsk.start();
            tsk.get();
        }
        REQUIRE(tester);
    }
    SECTION("eager co_awaited") {
        leak_tester tester;
        {
            auto tsk = coro(tester);
            tsk.start();
            auto encl = awaiter(std::move(tsk));
            encl.get();
        }
        REQUIRE(tester);
    }
}


TEST_CASE("Eager task: get value", "[Eager task]") {
    static const auto coro = [](int value) -> eager_task<int> {
        co_return value;
    };
    REQUIRE(coro(42).get() == 42);
}


TEST_CASE("Eager task: co_await value", "[Eager task]") {
    static const auto coro = [](int value) -> eager_task<int> {
        co_return value;
    };
    static const auto enclosing = [](int value) -> eager_task<int> {
        co_return co_await coro(value);
    };
    REQUIRE(enclosing(42).get() == 42);
}


TEST_CASE("Eager task: get ref", "[Eager task]") {
    static const auto coro = [](int value) -> eager_task<int> {
        co_return value;
    };
    REQUIRE(coro(42).get() == 42);
}


TEST_CASE("Eager task: co_await ref", "[Eager task]") {
    static int value = 42;
    static const auto coro = [](int& value) -> eager_task<int&> {
        co_return value;
    };
    static const auto enclosing = [](int& value) -> eager_task<int&> {
        co_return co_await coro(value);
    };
    auto eager_task = enclosing(value);
    auto& result = eager_task.get();
    REQUIRE(result == 42);
    REQUIRE(&result == &value);
}