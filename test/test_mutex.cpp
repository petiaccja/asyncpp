#include "monitor_task.hpp"

#include <asyncpp/mutex.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace asyncpp;


struct [[nodiscard]] mtx_scope_clear {
    ~mtx_scope_clear() {
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
    mtx_scope_clear guard(mtx);

    REQUIRE(mtx.try_lock());
    REQUIRE(mtx._debug_is_locked());
}


TEST_CASE("Mutex: lock direct immediate", "[Mutex]") {
    mutex mtx;
    mtx_scope_clear guard(mtx);

    auto monitor = lock_exclusively(mtx);
    REQUIRE(monitor.get_counters().done);
    REQUIRE(mtx._debug_is_locked());
}


TEST_CASE("Mutex: lock spurious immediate", "[Mutex]") {
    mutex mtx;
    mtx_scope_clear guard(mtx);

    auto monitor = []() -> monitor_task { co_return; }();
    auto awaiter = mtx.exclusive();
    REQUIRE(false == awaiter.await_suspend(monitor.handle()));
    REQUIRE(mtx._debug_is_locked());
}


TEST_CASE("Mutex: sequencial locking attempts", "[Mutex]") {
    mutex mtx;
    mtx_scope_clear guard(mtx);

    REQUIRE(mtx.try_lock());
    REQUIRE(!mtx.try_lock());
}


TEST_CASE("Mutex: unlock", "[Mutex]") {
    mutex mtx;
    mtx_scope_clear guard(mtx);

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
    mtx_scope_clear guard(mtx);

    unique_lock lk(mtx, std::defer_lock);
    REQUIRE(!lk.owns_lock());

    lk.try_lock();
    REQUIRE(lk.owns_lock());
    REQUIRE(mtx._debug_is_locked());
}


TEST_CASE("Mutex: unique lock await immediate", "[Mutex]") {
    mutex mtx;
    mtx_scope_clear guard(mtx);

    unique_lock lk(mtx, std::defer_lock);
    auto monitor = lock(lk);
    REQUIRE(monitor.get_counters().done);
    REQUIRE(lk.owns_lock());
    REQUIRE(mtx._debug_is_locked());
}


TEST_CASE("Mutex: unique lock await suspended", "[Mutex]") {
    mutex mtx;
    mtx_scope_clear guard(mtx);

    unique_lock lk(mtx, std::defer_lock);
    mtx.try_lock();
    auto monitor = lock(lk);
    mtx.unlock();
    REQUIRE(monitor.get_counters().done);
    REQUIRE(lk.owns_lock());
    REQUIRE(mtx._debug_is_locked());
}


TEST_CASE("Mutex: unique lock start locked", "[Mutex]") {
    mutex mtx;
    mtx_scope_clear guard(mtx);

    auto monitor = [](mutex& mtx) -> monitor_task {
        unique_lock lk(co_await mtx.exclusive());
        REQUIRE(lk.owns_lock());
        REQUIRE(mtx._debug_is_locked());
    }(mtx);

    REQUIRE(monitor.get_counters().done);
}


TEST_CASE("Mutex: unique lock unlock", "[Mutex]") {
    mutex mtx;
    mtx_scope_clear guard(mtx);

    unique_lock lk(mtx, std::defer_lock);
    lk.try_lock();
    lk.unlock();
    REQUIRE(!lk.owns_lock());
    REQUIRE(!mtx._debug_is_locked());
}


TEST_CASE("Mutex: unique lock destructor", "[Mutex]") {
    mutex mtx;
    mtx_scope_clear guard(mtx);

    {
        unique_lock lk(mtx, std::defer_lock);
        lk.try_lock();
    }

    REQUIRE(!mtx._debug_is_locked());
}


TEST_CASE("Mutex: unique lock move ctor", "[Mutex]") {
    mutex mtx;
    mtx_scope_clear guard(mtx);

    {
        unique_lock lk(mtx, std::defer_lock);
        lk.try_lock();
        REQUIRE(mtx._debug_is_locked());
        unique_lock copy{ std::move(lk) };
        REQUIRE(!lk.owns_lock());
        REQUIRE(mtx._debug_is_locked());
    }

    REQUIRE(!mtx._debug_is_locked());
}


TEST_CASE("Mutex: unique lock move assign", "[Mutex]") {
    mutex mtx1;
    mutex mtx2;
    mtx_scope_clear guard1(mtx1);
    mtx_scope_clear guard2(mtx2);

    {
        unique_lock lk1(mtx1, std::defer_lock);
        unique_lock lk2(mtx2, std::defer_lock);
        lk1.try_lock();
        lk2.try_lock();
        REQUIRE(mtx1._debug_is_locked());
        REQUIRE(mtx2._debug_is_locked());
        lk1 = std::move(lk2);
        REQUIRE(!lk2.owns_lock());
        REQUIRE(!mtx1._debug_is_locked());
        REQUIRE(mtx2._debug_is_locked());
    }

    REQUIRE(!mtx1._debug_is_locked());
    REQUIRE(!mtx2._debug_is_locked());
}