#include <async++/interleaving/runner.hpp>
#include <async++/join.hpp>
#include <async++/mutex.hpp>
#include <async++/task.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace asyncpp;


TEST_CASE("Mutex: try lock", "[Mutex]") {
    static const auto coro = [](mutex& mtx) -> task<void> {
        REQUIRE(mtx.try_lock());
        REQUIRE(!mtx.try_lock());
        mtx.unlock();
        co_return;
    };

    mutex mtx;
    join(coro(mtx));
}


TEST_CASE("Mutex: lock", "[Mutex]") {
    static const auto coro = [](mutex& mtx) -> task<void> {
        co_await mtx;
        REQUIRE(!mtx.try_lock());
        mtx.unlock();
    };

    mutex mtx;
    join(coro(mtx));
}


TEST_CASE("Mutex: unlock", "[Mutex]") {
    static const auto coro = [](mutex& mtx) -> task<void> {
        co_await mtx;
        mtx.unlock();
        REQUIRE(mtx.try_lock());
        mtx.unlock();
    };

    mutex mtx;
    join(coro(mtx));
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
    join(coro(mtx));
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
    join(coro(mtx));
}


TEST_CASE("Mutex: unique lock start locked", "[Mutex]") {
    static const auto coro = [](mutex& mtx) -> task<void> {
        unique_lock lk(co_await mtx);
        REQUIRE(lk.owns_lock());
        co_return;
    };

    mutex mtx;
    join(coro(mtx));
}


TEST_CASE("Mutex: unique lock unlock", "[Mutex]") {
    static const auto coro = [](mutex& mtx) -> task<void> {
        unique_lock lk(co_await mtx);
        lk.unlock();
        REQUIRE(!lk.owns_lock());
        co_return;
    };

    mutex mtx;
    join(coro(mtx));
}


TEST_CASE("Mutex: unique lock destroy", "[Shared mutex]") {
    static const auto coro = [](mutex& mtx) -> task<void> {
        {
            unique_lock lk(co_await mtx);
            REQUIRE(lk.owns_lock());
        }
        REQUIRE(mtx.try_lock());
        mtx.unlock();
        co_return;
    };

    mutex mtx;
    join(coro(mtx));
}


TEST_CASE("Mutex: resume awaiting", "[Mutex]") {
    static const auto awaiter = [](mutex& mtx, std::vector<int>& sequence, int id) -> task<void> {
        co_await mtx;
        sequence.push_back(id);
        mtx.unlock();
    };
    static const auto main = [](mutex& mtx, std::vector<int>& sequence) -> task<void> {
        auto t1 = awaiter(mtx, sequence, 1);
        auto t2 = awaiter(mtx, sequence, 2);

        co_await mtx;
        sequence.push_back(0);
        t1.launch();
        t2.launch();
        mtx.unlock();
    };

    mutex mtx;
    std::vector<int> sequence;
    join(main(mtx, sequence));
    REQUIRE(sequence == std::vector{ 0, 1, 2 });
}