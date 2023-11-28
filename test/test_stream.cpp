#include "helper_leak_tester.hpp"

#include <async++/join.hpp>
#include <async++/stream.hpp>

#include <vector>

#include <catch2/catch_test_macros.hpp>


using namespace asyncpp;


TEST_CASE("Stream: destroy", "[Task]") {
    static const auto coro = [](leak_tester value) -> stream<int> { co_yield 0; };

    SECTION("no execution") {
        leak_tester tester;
        {
            auto s = coro(tester);
        }
        REQUIRE(tester);
    }
    SECTION("synced") {
        leak_tester tester;
        {
            auto s = coro(tester);
            join(s);
        }
        REQUIRE(tester);
    }
}


TEST_CASE("Stream: co_await", "[Generator]") {
    static const auto coro = [](int count) -> stream<int> {
        static int i;
        for (i = 0; i < count; ++i) {
            co_yield i;
        }
    };
    static const auto enclosing = [](int count) -> stream<std::vector<int>> {
        std::vector<int> values;
        const auto s = coro(count);
        while (const auto value = co_await s) {
            values.push_back(*value);
        }
        co_yield values;
    };
    const auto results = join(enclosing(4));
    REQUIRE(results == std::vector{ 0, 1, 2, 3 });
}


TEST_CASE("Stream: co_await - reference", "[Generator]") {
    static const auto coro = [](int count) -> stream<int&> {
        static int i;
        for (i = 0; i < count; ++i) {
            co_yield i;
        }
    };
    static const auto enclosing = [](int count) -> stream<std::vector<int>> {
        std::vector<int> values;
        const auto s = coro(count);
        while (const auto value = co_await s) {
            values.push_back(*value);
            ++value->get();
        }
        co_yield values;
    };
    const auto results = join(enclosing(8));
    REQUIRE(results == std::vector{ 0, 2, 4, 6 });
}