#include "monitor_task.hpp"

#include <asyncpp/join.hpp>
#include <asyncpp/stream.hpp>

#include <catch2/catch_test_macros.hpp>


using namespace asyncpp;


TEST_CASE("Stream: creation", "[Stream]") {
    const auto coro = []() -> stream<int> {
        co_yield 0;
    };

    SECTION("empty") {
        stream<int> s;
        REQUIRE(!s.valid());
    }
    SECTION("valid") {
        auto s = coro();
        REQUIRE(s.valid());
    }
}


TEST_CASE("Stream: iteration", "[Stream]") {
    static const auto coro = []() -> stream<int> {
        co_yield 0;
        co_yield 1;
    };

    auto s = coro();
    auto r = [&]() -> monitor_task {
        auto i1 = co_await s;
        REQUIRE(i1);
        REQUIRE(*i1 == 0);
        auto i2 = co_await s;
        REQUIRE(i2);
        REQUIRE(*i2 == 1);
        auto i3 = co_await s;
        REQUIRE(!i3);
    }();
    REQUIRE(r.get_counters().done);
}


TEST_CASE("Stream: data types", "[Stream]") {
    SECTION("value") {
        static const auto coro = []() -> stream<int> {
            co_yield 0;
        };
        auto r = []() -> monitor_task {
            auto s = coro();
            auto item = co_await s;
            REQUIRE(*item == 0);
        }();
        REQUIRE(r.get_counters().done);
    }
    SECTION("reference") {
        static int value = 0;
        static const auto coro = []() -> stream<int&> {
            co_yield value;
        };
        auto r = []() -> monitor_task {
            auto s = coro();
            auto item = co_await s;
            REQUIRE(&*item == &value);
        }();
        REQUIRE(r.get_counters().done);
    }
    SECTION("exception") {
        static const auto coro = []() -> stream<int> {
            throw std::runtime_error("test");
            co_return;
        };
        auto r = []() -> monitor_task {
            auto s = coro();
            REQUIRE_THROWS_AS(co_await s, std::runtime_error);
        }();
        REQUIRE(r.get_counters().done);
    }
}


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
            void(join(s));
        }
        REQUIRE(impl::leak_checked_promise::check(before));
    }
}
