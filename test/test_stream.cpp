#include <asyncpp/join.hpp>
#include <asyncpp/stream.hpp>

#include <vector>

#include <catch2/catch_test_macros.hpp>


using namespace asyncpp;


TEST_CASE("Stream: destroy", "[Task]") {
    static const auto coro = []() -> stream<int> { co_yield 0; };

    SECTION("no execution") {
        const auto before = impl::leak_checked_promise::snapshot();
        {
            auto s = coro();
        }
        REQUIRE(impl::leak_checked_promise::check(before));
    }
    SECTION("synced") {
        const auto before = impl::leak_checked_promise::snapshot();
        {
            auto s = coro();
            join(s);
        }
        REQUIRE(impl::leak_checked_promise::check(before));
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