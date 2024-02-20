#include "monitor_task.hpp"

#include <asyncpp/mutex.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace asyncpp;


struct [[nodiscard]] scope_clear {
    ~scope_clear() {
        mtx._debug_clear();
    }
    mutex& mtx;
};


static monitor_task lock_exclusively(mutex& mtx) {
    co_await mtx.exclusive();
}


static monitor_task lock(unique_lock<mutex>& lk) {
    co_await lk;
}


TEST_CASE("Mutex: try lock", "[Mutex]") {
    mutex mtx;
    scope_clear guard(mtx);

    REQUIRE(mtx.try_lock());
    REQUIRE(mtx._debug_is_locked());
}


TEST_CASE("Mutex: lock direct immediate", "[Mutex]") {
    mutex mtx;
    scope_clear guard(mtx);

    auto monitor = lock_exclusively(mtx);
    REQUIRE(monitor.get_counters().done);
    REQUIRE(mtx._debug_is_locked());
}


TEST_CASE("Mutex: lock spurious immediate", "[Mutex]") {
    mutex mtx;
    scope_clear guard(mtx);

    auto monitor = []() -> monitor_task { co_return; }();
    auto awaiter = mtx.exclusive();
    REQUIRE(false == awaiter.await_suspend(monitor.handle()));
    REQUIRE(mtx._debug_is_locked());
}


TEST_CASE("Mutex: sequencial locking attempts", "[Mutex]") {
    mutex mtx;
    scope_clear guard(mtx);

    REQUIRE(mtx.try_lock());
    REQUIRE(!mtx.try_lock());
}


TEST_CASE("Mutex: unlock", "[Mutex]") {
    mutex mtx;
    scope_clear guard(mtx);

    SECTION("exclusive -> free") {
        mtx.try_lock();
        mtx.unlock();
        REQUIRE(!mtx._debug_is_locked());
    }
    SECTION("locked -> exclusive") {
        mtx.try_lock();

        auto monitor1 = lock_exclusively(mtx);
        auto monitor2 = lock_exclusively(mtx);

        REQUIRE(!monitor1.get_counters().done);
        REQUIRE(!monitor2.get_counters().done);

        mtx.unlock();

        REQUIRE(monitor1.get_counters().done);
        REQUIRE(!monitor2.get_counters().done);
    }
}


TEST_CASE("Mutex: unique lock try_lock", "[Mutex]") {
    mutex mtx;
    scope_clear guard(mtx);

    unique_lock lk(mtx);
    REQUIRE(!lk.owns_lock());

    lk.try_lock();
    REQUIRE(lk.owns_lock());
    REQUIRE(mtx._debug_is_locked());
}


TEST_CASE("Mutex: unique lock await", "[Mutex]") {
    mutex mtx;
    scope_clear guard(mtx);

    unique_lock lk(mtx);
    auto monitor = lock(lk);
    REQUIRE(monitor.get_counters().done);
    REQUIRE(lk.owns_lock());
    REQUIRE(mtx._debug_is_locked());
}


TEST_CASE("Mutex: unique lock start locked", "[Mutex]") {
    mutex mtx;
    scope_clear guard(mtx);

    auto monitor = [](mutex& mtx) -> monitor_task {
        unique_lock lk(co_await mtx.exclusive());
        REQUIRE(lk.owns_lock());
        REQUIRE(mtx._debug_is_locked());
    }(mtx);

    REQUIRE(monitor.get_counters().done);
}


TEST_CASE("Mutex: unique lock unlock", "[Mutex]") {
    mutex mtx;
    scope_clear guard(mtx);

    unique_lock lk(mtx);
    lk.try_lock();
    lk.unlock();
    REQUIRE(!lk.owns_lock());
    REQUIRE(!mtx._debug_is_locked());
}


TEST_CASE("Mutex: unique lock destructor", "[Mutex]") {
    mutex mtx;
    scope_clear guard(mtx);

    {
        unique_lock lk(mtx);
        lk.try_lock();
    }

    REQUIRE(!mtx._debug_is_locked());
}