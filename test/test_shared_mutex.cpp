#include "monitor_task.hpp"

#include <asyncpp/join.hpp>
#include <asyncpp/shared_mutex.hpp>
#include <asyncpp/task.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace asyncpp;


struct [[nodiscard]] scope_clear {
    ~scope_clear() {
        mtx._debug_clear();
    }
    shared_mutex& mtx;
};


static monitor_task lock_exclusively(shared_mutex& mtx) {
    co_await mtx.exclusive();
}


static monitor_task lock_shared(shared_mutex& mtx) {
    co_await mtx.shared();
}


static monitor_task lock(unique_lock<shared_mutex>& lk) {
    co_await lk;
}


static monitor_task lock(shared_lock<shared_mutex>& lk) {
    co_await lk;
}


TEST_CASE("Shared mutex: try lock", "[Shared mutex]") {
    shared_mutex mtx;
    scope_clear guard(mtx);

    SECTION("exclusive") {
        REQUIRE(mtx.try_lock());
        REQUIRE(mtx._debug_is_exclusive_locked());
    }

    SECTION("shared") {
        REQUIRE(mtx.try_lock_shared());
        REQUIRE(mtx._debug_is_shared_locked());
    }
}


TEST_CASE("Shared mutex: lock", "[Shared mutex]") {
    shared_mutex mtx;
    scope_clear guard(mtx);

    SECTION("exclusive") {
        auto monitor = lock_exclusively(mtx);
        REQUIRE(monitor.get_counters().done);
        REQUIRE(mtx._debug_is_exclusive_locked());
    }
    SECTION("shared") {
        auto monitor = lock_shared(mtx);
        REQUIRE(monitor.get_counters().done);
        REQUIRE(mtx._debug_is_shared_locked());
    }
}


TEST_CASE("Shared mutex: sequencial locking attempts", "[Shared mutex]") {
    shared_mutex mtx;
    scope_clear guard(mtx);

    SECTION("exclusive - exclusive") {
        REQUIRE(mtx.try_lock());
        REQUIRE(!mtx.try_lock());
    }
    SECTION("exclusive - shared") {
        REQUIRE(mtx.try_lock());
        REQUIRE(!mtx.try_lock_shared());
    }
    SECTION("shared - exclusive") {
        REQUIRE(mtx.try_lock_shared());
        REQUIRE(!mtx.try_lock());
    }
    SECTION("shared - shared") {
        REQUIRE(mtx.try_lock_shared());
        REQUIRE(mtx.try_lock_shared());
    }
}


TEST_CASE("Shared mutex: unlock", "[Shared mutex]") {
    shared_mutex mtx;
    scope_clear guard(mtx);

    SECTION("exclusive -> free") {
        mtx.try_lock();
        mtx.unlock();
        REQUIRE(!mtx._debug_is_exclusive_locked());
        REQUIRE(!mtx._debug_is_shared_locked());
    }
    SECTION("shared * n -> free") {
        mtx.try_lock_shared();
        mtx.try_lock_shared();
        mtx.unlock_shared();
        REQUIRE(!mtx._debug_is_exclusive_locked());
        REQUIRE(mtx._debug_is_shared_locked());
        mtx.unlock_shared();
        REQUIRE(!mtx._debug_is_exclusive_locked());
        REQUIRE(!mtx._debug_is_shared_locked());
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
    SECTION("locked -> shared * n") {
        mtx.try_lock();

        auto monitor1 = lock_shared(mtx);
        auto monitor2 = lock_shared(mtx);
        auto monitor3 = lock_exclusively(mtx);

        REQUIRE(!monitor1.get_counters().done);
        REQUIRE(!monitor2.get_counters().done);
        REQUIRE(!monitor3.get_counters().done);

        mtx.unlock();

        REQUIRE(monitor1.get_counters().done);
        REQUIRE(monitor2.get_counters().done);
        REQUIRE(!monitor3.get_counters().done);
    }
}


TEST_CASE("Shared mutex: unique lock try_lock", "[Shared mutex]") {
    shared_mutex mtx;
    scope_clear guard(mtx);

    unique_lock lk(mtx);
    REQUIRE(!lk.owns_lock());

    lk.try_lock();
    REQUIRE(lk.owns_lock());
    REQUIRE(mtx._debug_is_exclusive_locked());
}


TEST_CASE("Shared mutex: unique lock await", "[Shared mutex]") {
    shared_mutex mtx;
    scope_clear guard(mtx);

    unique_lock lk(mtx);
    auto monitor = lock(lk);
    REQUIRE(monitor.get_counters().done);
    REQUIRE(lk.owns_lock());
    REQUIRE(mtx._debug_is_exclusive_locked());
}


TEST_CASE("Shared mutex: unique lock start locked", "[Shared mutex]") {
    shared_mutex mtx;
    scope_clear guard(mtx);

    auto monitor = [](shared_mutex& mtx) -> monitor_task {
        unique_lock lk(co_await mtx.exclusive());
        REQUIRE(lk.owns_lock());
        REQUIRE(mtx._debug_is_exclusive_locked());
    }(mtx);

    REQUIRE(monitor.get_counters().done);
}


TEST_CASE("Shared mutex: unique lock unlock", "[Shared mutex]") {
    shared_mutex mtx;
    scope_clear guard(mtx);

    unique_lock lk(mtx);
    lk.try_lock();
    lk.unlock();
    REQUIRE(!lk.owns_lock());
    REQUIRE(!mtx._debug_is_exclusive_locked());
}


TEST_CASE("Shared mutex: unique lock destructor", "[Shared mutex]") {
    shared_mutex mtx;
    scope_clear guard(mtx);

    {
        unique_lock lk(mtx);
        lk.try_lock();
    }

    REQUIRE(!mtx._debug_is_exclusive_locked());
}



TEST_CASE("Shared mutex: shared lock try_lock", "[Shared mutex]") {
    shared_mutex mtx;
    scope_clear guard(mtx);

    shared_lock lk(mtx);
    REQUIRE(!lk.owns_lock());

    lk.try_lock();
    REQUIRE(lk.owns_lock());
    REQUIRE(mtx._debug_is_shared_locked());
}


TEST_CASE("Shared mutex: shared lock await", "[Shared mutex]") {
    shared_mutex mtx;
    scope_clear guard(mtx);

    shared_lock lk(mtx);
    auto monitor = lock(lk);
    REQUIRE(monitor.get_counters().done);
    REQUIRE(lk.owns_lock());
    REQUIRE(mtx._debug_is_shared_locked());
}


TEST_CASE("Shared mutex: shared lock start locked", "[Shared mutex]") {
    shared_mutex mtx;
    scope_clear guard(mtx);

    auto monitor = [](shared_mutex& mtx) -> monitor_task {
        shared_lock lk(co_await mtx.shared());
        REQUIRE(lk.owns_lock());
        REQUIRE(mtx._debug_is_shared_locked());
    }(mtx);

    REQUIRE(monitor.get_counters().done);
}


TEST_CASE("Shared mutex: shared lock unlock", "[Shared mutex]") {
    shared_mutex mtx;
    scope_clear guard(mtx);

    shared_lock lk(mtx);
    lk.try_lock();
    lk.unlock();
    REQUIRE(!lk.owns_lock());
    REQUIRE(!mtx._debug_is_shared_locked());
}


TEST_CASE("Shared mutex: shared lock destructor", "[Shared mutex]") {
    shared_mutex mtx;
    scope_clear guard(mtx);

    {
        shared_lock lk(mtx);
        lk.try_lock();
    }

    REQUIRE(!mtx._debug_is_shared_locked());
}