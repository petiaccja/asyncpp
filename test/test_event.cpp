#include "helper_schedulers.hpp"
#include "monitor_task.hpp"

#include <asyncpp/event.hpp>
#include <asyncpp/task.hpp>
#include <asyncpp/testing/interleaver.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace asyncpp;


TEST_CASE("Event: set value", "[Event]") {
    broadcast_event<int> evt;
    REQUIRE(evt._debug_get_result().has_value() == false);
    evt.set_value(1);
    REQUIRE(evt._debug_get_result().get_or_throw() == 1);
}


TEST_CASE("Event: set error", "[Event]") {
    broadcast_event<int> evt;
    REQUIRE(evt._debug_get_result().has_value() == false);
    evt.set_exception(std::make_exception_ptr(std::runtime_error("test")));
    REQUIRE_THROWS_AS(evt._debug_get_result().get_or_throw(), std::runtime_error);
}


TEST_CASE("Event: set twice", "[Event]") {
    broadcast_event<int> evt;
    REQUIRE(evt._debug_get_result().has_value() == false);
    evt.set_value(1);
    REQUIRE_THROWS(evt.set_value(1));
    REQUIRE(evt._debug_get_result().get_or_throw() == 1);
}


template <class Event>
monitor_task monitor_coro(Event& evt) {
    co_await evt;
}


TEST_CASE("Event: await before set", "[Event]") {
    broadcast_event<int> evt;

    SECTION("value") {
        auto monitor = monitor_coro(evt);
        REQUIRE(monitor.get_counters().done == false);
        evt.set_value(1);
        REQUIRE(monitor.get_counters().suspensions == 1);
        REQUIRE(monitor.get_counters().done == true);
        REQUIRE(monitor.get_counters().exception == nullptr);
    }
    SECTION("exception") {
        auto monitor = monitor_coro(evt);
        REQUIRE(monitor.get_counters().done == false);
        evt.set_exception(std::make_exception_ptr(std::runtime_error("test")));
        REQUIRE(monitor.get_counters().suspensions == 1);
        REQUIRE(monitor.get_counters().done == true);
        REQUIRE(monitor.get_counters().exception != nullptr);
    }
}


TEST_CASE("Event: await after set", "[Event]") {
    broadcast_event<int> evt;

    SECTION("value") {
        evt.set_value(1);
        auto monitor = monitor_coro(evt);
        REQUIRE(monitor.get_counters().suspensions == 0);
        REQUIRE(monitor.get_counters().done == true);
        REQUIRE(monitor.get_counters().exception == nullptr);
    }
    SECTION("exception") {
        evt.set_exception(std::make_exception_ptr(std::runtime_error("test")));
        auto monitor = monitor_coro(evt);
        REQUIRE(monitor.get_counters().suspensions == 0);
        REQUIRE(monitor.get_counters().done == true);
        REQUIRE(monitor.get_counters().exception != nullptr);
    }
}


TEST_CASE("Event: multiple awaiters", "[Event]") {
    broadcast_event<int> evt;

    auto mon1 = monitor_coro(evt);
    auto mon2 = monitor_coro(evt);
    evt.set_value(1);
    auto mon3 = monitor_coro(evt);
    auto mon4 = monitor_coro(evt);

    REQUIRE(mon1.get_counters().suspensions == 1);
    REQUIRE(mon2.get_counters().suspensions == 1);
    REQUIRE(mon3.get_counters().suspensions == 0);
    REQUIRE(mon4.get_counters().suspensions == 0);

    REQUIRE(mon1.get_counters().done == true);
    REQUIRE(mon2.get_counters().done == true);
    REQUIRE(mon3.get_counters().done == true);
    REQUIRE(mon4.get_counters().done == true);

    REQUIRE(mon1.get_counters().exception == nullptr);
    REQUIRE(mon2.get_counters().exception == nullptr);
    REQUIRE(mon3.get_counters().exception == nullptr);
    REQUIRE(mon4.get_counters().exception == nullptr);
}
