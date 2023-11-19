#include "leak_tester.hpp"

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
            void(s.get());
        }
        REQUIRE(tester);
    }
}


TEST_CASE("Stream: get", "[Stream]") {
    static const auto coro = [](int count) -> stream<int> {
        for (int i = 0; i < count; ++i) {
            co_yield i;
        }
    };
    const auto s = coro(4);
    std::vector<int> results;
    while (const auto value = s.get()) {
        results.push_back(*value);
    }
    REQUIRE(results == std::vector{ 0, 1, 2, 3 });
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
    const auto results = enclosing(4).get();
    REQUIRE(results == std::vector{ 0, 1, 2, 3 });
}


TEST_CASE("Stream: get - reference", "[Stream]") {
    static const auto coro = [](int count) -> stream<int&> {
        static int i;
        for (i = 0; i < count; ++i) {
            co_yield i;
        }
    };
    const auto s = coro(8);
    std::vector<int> results;
    while (const auto value = s.get()) {
        results.push_back(*value);
        ++value->get();
    }
    REQUIRE(results == std::vector{ 0, 2, 4, 6 });
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
    const auto results = enclosing(8).get();
    REQUIRE(results == std::vector{ 0, 2, 4, 6 });
}