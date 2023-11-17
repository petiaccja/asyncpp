#include <async++/mutex.hpp>
#include <async++/task.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace asyncpp;


TEST_CASE("Mutex: try lock", "[Mutex]") {
    static const auto coro = [](mutex& mtx) -> task<void> {
        const auto lock = mtx.try_lock();
        REQUIRE(!mtx.try_lock());
        co_return;
    };

    mutex mtx;
    coro(mtx).get();
}


TEST_CASE("Mutex: lock", "[Mutex]") {
    static const auto coro = [](mutex& mtx) -> task<void> {
        const auto lock = co_await mtx;
        REQUIRE(!mtx.try_lock());
    };

    mutex mtx;
    coro(mtx).get();
}


TEST_CASE("Mutex: unique lock try", "[Mutex]") {
    static const auto coro = [](mutex& mtx) -> task<void> {
        unique_lock lk(mtx);
        REQUIRE(!lk.owns_lock());
        REQUIRE(lk.try_lock());
        REQUIRE(lk.owns_lock());
        co_return;
    };

    mutex mtx;
    coro(mtx).get();
}


TEST_CASE("Mutex: unique lock await", "[Mutex]") {
    static const auto coro = [](mutex& mtx) -> task<void> {
        unique_lock lk(mtx);
        REQUIRE(!lk.owns_lock());
        co_await lk;
        REQUIRE(lk.owns_lock());
        co_return;
    };

    mutex mtx;
    coro(mtx).get();
}


TEST_CASE("Mutex: unique lock start locked", "[Mutex]") {
    static const auto coro = [](mutex& mtx) -> task<void> {
        unique_lock lk(co_await mtx);
        REQUIRE(lk.owns_lock());
        co_return;
    };

    mutex mtx;
    coro(mtx).get();
}