#include "monitor_task.hpp"

#include <asyncpp/semaphore.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace asyncpp;


struct [[nodiscard]] sema_scope_clear {
    ~sema_scope_clear() {
        sema._debug_clear();
    }
    counting_semaphore& sema;
};


static monitor_task acquire(counting_semaphore& sema) {
    co_await sema;
}


TEST_CASE("Semaphore - try_acquire", "[Semaphore]") {
    SECTION("success") {
        counting_semaphore sema(1);
        REQUIRE(sema.try_acquire());
        REQUIRE(sema._debug_get_awaiters().empty());
        REQUIRE(sema._debug_get_counter() == 0);
    }
    SECTION("failure") {
        counting_semaphore sema(0);
        REQUIRE(!sema.try_acquire());
        REQUIRE(sema._debug_get_awaiters().empty());
        REQUIRE(sema._debug_get_counter() == 0);
    }
}


TEST_CASE("Semaphore - acquire direct immediate", "[Semaphore]") {
    SECTION("success") {
        counting_semaphore sema(1);
        sema_scope_clear guard(sema);
        auto monitor = acquire(sema);
        REQUIRE(monitor.get_counters().done);
        REQUIRE(sema._debug_get_awaiters().empty());
        REQUIRE(sema._debug_get_counter() == 0);
    }
    SECTION("failure") {
        counting_semaphore sema(0);
        sema_scope_clear guard(sema);
        auto monitor = acquire(sema);
        REQUIRE(!monitor.get_counters().done);
        REQUIRE(!sema._debug_get_awaiters().empty());
        REQUIRE(sema._debug_get_counter() == 0);
    }
}


TEST_CASE("Semaphore - acquire spurious immediate", "[Semaphore]") {
    SECTION("success") {
        counting_semaphore sema(1);
        sema_scope_clear guard(sema);

        auto monitor = []() -> monitor_task { co_return; }();
        auto awaiter = sema.operator co_await();

        REQUIRE(false == awaiter.await_suspend(monitor.handle()));
        REQUIRE(sema._debug_get_awaiters().empty());
        REQUIRE(sema._debug_get_counter() == 0);
    }
    SECTION("failure") {
        counting_semaphore sema(0);
        sema_scope_clear guard(sema);

        auto monitor = []() -> monitor_task { co_return; }();
        auto awaiter = sema.operator co_await();

        REQUIRE(true == awaiter.await_suspend(monitor.handle()));
        REQUIRE(!sema._debug_get_awaiters().empty());
        REQUIRE(sema._debug_get_counter() == 0);
    }
}


TEST_CASE("Semaphore - release", "[Semaphore]") {
    counting_semaphore sema(0);

    auto monitor1 = acquire(sema);
    auto monitor2 = acquire(sema);

    REQUIRE(!monitor1.get_counters().done);
    REQUIRE(!monitor2.get_counters().done);

    sema.release();

    REQUIRE(monitor1.get_counters().done);
    REQUIRE(!monitor2.get_counters().done);

    sema.release();

    REQUIRE(monitor1.get_counters().done);
    REQUIRE(monitor2.get_counters().done);
}
