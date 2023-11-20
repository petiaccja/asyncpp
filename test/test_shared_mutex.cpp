#include <async++/interleaving/runner.hpp>
#include <async++/join.hpp>
#include <async++/shared_mutex.hpp>
#include <async++/task.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace asyncpp;


TEST_CASE("Shared mutex: try lock", "[Shared mutex]") {
    static const auto coro = [](shared_mutex& mtx) -> task<void> {
        REQUIRE(mtx.try_lock());
        REQUIRE(!mtx.try_lock());
        REQUIRE(!mtx.try_lock_shared());
        co_return;
    };

    shared_mutex mtx;
    join(coro(mtx));
}


TEST_CASE("Shared mutex: lock", "[Shared mutex]") {
    static const auto coro = [](shared_mutex& mtx) -> task<void> {
        co_await mtx.unique();
        REQUIRE(!mtx.try_lock());
        REQUIRE(!mtx.try_lock_shared());
    };

    shared_mutex mtx;
    join(coro(mtx));
}


TEST_CASE("Shared mutex: unlock", "[Shared mutex]") {
    static const auto coro = [](shared_mutex& mtx) -> task<void> {
        co_await mtx.unique();
        mtx.unlock();
        REQUIRE(mtx.try_lock());
        mtx.unlock();
        REQUIRE(mtx.try_lock_shared());
    };

    shared_mutex mtx;
    join(coro(mtx));
}


TEST_CASE("Shared mutex: try lock shared", "[Shared mutex]") {
    static const auto coro = [](shared_mutex& mtx) -> task<void> {
        REQUIRE(mtx.try_lock_shared());
        REQUIRE(!mtx.try_lock());
        REQUIRE(mtx.try_lock_shared());
        co_return;
    };

    shared_mutex mtx;
    join(coro(mtx));
}


TEST_CASE("Shared mutex: lock shared", "[Shared mutex]") {
    static const auto coro = [](shared_mutex& mtx) -> task<void> {
        co_await mtx.shared();
        REQUIRE(!mtx.try_lock());
        REQUIRE(mtx.try_lock_shared());
    };

    shared_mutex mtx;
    join(coro(mtx));
}


TEST_CASE("Shared mutex: unlock shared", "[Shared mutex]") {
    static const auto coro = [](shared_mutex& mtx) -> task<void> {
        co_await mtx.shared();
        mtx.unlock_shared();
        REQUIRE(mtx.try_lock());
        mtx.unlock();
        REQUIRE(mtx.try_lock_shared());
    };

    shared_mutex mtx;
    join(coro(mtx));
}


TEST_CASE("Shared mutex: unique lock try", "[Shared mutex]") {
    static const auto coro = [](shared_mutex& mtx) -> task<void> {
        unique_lock lk(mtx);
        REQUIRE(!lk.owns_lock());
        REQUIRE(lk.try_lock());
        REQUIRE(lk.owns_lock());
        co_return;
    };

    shared_mutex mtx;
    join(coro(mtx));
}


TEST_CASE("Shared mutex: unique lock await", "[Shared mutex]") {
    static const auto coro = [](shared_mutex& mtx) -> task<void> {
        unique_lock lk(mtx);
        REQUIRE(!lk.owns_lock());
        co_await lk;
        REQUIRE(lk.owns_lock());
        co_return;
    };

    shared_mutex mtx;
    join(coro(mtx));
}


TEST_CASE("Shared mutex: unique lock start locked", "[Shared mutex]") {
    static const auto coro = [](shared_mutex& mtx) -> task<void> {
        unique_lock lk(co_await mtx.unique());
        REQUIRE(lk.owns_lock());
        co_return;
    };

    shared_mutex mtx;
    join(coro(mtx));
}


TEST_CASE("Shared mutex: unique lock unlock", "[Shared mutex]") {
    static const auto coro = [](shared_mutex& mtx) -> task<void> {
        unique_lock lk(co_await mtx.unique());
        lk.unlock();
        REQUIRE(!lk.owns_lock());
        co_return;
    };

    shared_mutex mtx;
    join(coro(mtx));
}


TEST_CASE("Shared mutex: shared lock try", "[Shared mutex]") {
    static const auto coro = [](shared_mutex& mtx) -> task<void> {
        shared_lock lk(mtx);
        REQUIRE(!lk.owns_lock());
        REQUIRE(lk.try_lock());
        REQUIRE(lk.owns_lock());
        co_return;
    };

    shared_mutex mtx;
    join(coro(mtx));
}


TEST_CASE("Shared mutex: shared lock await", "[Shared mutex]") {
    static const auto coro = [](shared_mutex& mtx) -> task<void> {
        shared_lock lk(mtx);
        REQUIRE(!lk.owns_lock());
        co_await lk;
        REQUIRE(lk.owns_lock());
        co_return;
    };

    shared_mutex mtx;
    join(coro(mtx));
}


TEST_CASE("Shared mutex: shared lock start locked", "[Shared mutex]") {
    static const auto coro = [](shared_mutex& mtx) -> task<void> {
        shared_lock lk(co_await mtx.shared());
        REQUIRE(lk.owns_lock());
        co_return;
    };

    shared_mutex mtx;
    join(coro(mtx));
}


TEST_CASE("Shared mutex: shared lock unlock", "[Shared mutex]") {
    static const auto coro = [](shared_mutex& mtx) -> task<void> {
        shared_lock lk(co_await mtx.shared());
        REQUIRE(lk.owns_lock());
        lk.unlock();
        REQUIRE(!lk.owns_lock());
        co_return;
    };

    shared_mutex mtx;
    join(coro(mtx));
}


TEST_CASE("Shared mutex: shared lock destroy", "[Shared mutex]") {
    static const auto coro = [](shared_mutex& mtx) -> task<void> {
        {
            shared_lock lk(co_await mtx.shared());
            REQUIRE(lk.owns_lock());
        }
        REQUIRE(mtx.try_lock());
        co_return;
    };

    shared_mutex mtx;
    join(coro(mtx));
}


TEST_CASE("Shared mutex: resume awaiting", "[Shared mutex]") {
    static const auto awaiter = [](shared_mutex& mtx, std::vector<int>& sequence, int id) -> task<void> {
        co_await mtx.unique();
        sequence.push_back(id);
        mtx.unlock();
    };
    static const auto shared_awaiter = [](shared_mutex& mtx, std::vector<int>& sequence, int id) -> task<void> {
        co_await mtx.shared();
        sequence.push_back(id);
        mtx.unlock_shared();
    };
    static const auto main = [](shared_mutex& mtx, std::vector<int>& sequence) -> task<void> {
        auto s1 = shared_awaiter(mtx, sequence, 1);
        auto s2 = shared_awaiter(mtx, sequence, 2);
        auto u1 = awaiter(mtx, sequence, -1);
        auto u2 = awaiter(mtx, sequence, -2);

        co_await mtx.unique();
        sequence.push_back(0);
        s1.launch();
        s2.launch();
        u1.launch();
        u2.launch();
        mtx.unlock();
    };

    shared_mutex mtx;
    std::vector<int> sequence;
    join(main(mtx, sequence));
    REQUIRE(sequence == std::vector{ 0, 1, 2, -1, -2 });
}


TEST_CASE("Shared mutex: unique starvation", "[Shared mutex]") {
    static const auto awaiter = [](shared_mutex& mtx, std::vector<int>& sequence, int id) -> task<void> {
        co_await mtx.unique();
        sequence.push_back(id);
        mtx.unlock();
    };
    static const auto shared_awaiter = [](shared_mutex& mtx, std::vector<int>& sequence, int id) -> task<void> {
        co_await mtx.shared();
        sequence.push_back(id);
        mtx.unlock_shared();
    };
    static const auto main = [](shared_mutex& mtx, std::vector<int>& sequence) -> task<void> {
        auto s1 = shared_awaiter(mtx, sequence, 1);
        auto s2 = shared_awaiter(mtx, sequence, 2);
        auto s3 = shared_awaiter(mtx, sequence, 3);
        auto s4 = shared_awaiter(mtx, sequence, 4);
        auto u1 = awaiter(mtx, sequence, -1);

        co_await mtx.shared();
        sequence.push_back(0);
        s1.launch();
        s2.launch();
        u1.launch();
        s3.launch();
        mtx.unlock_shared();
        co_await mtx.shared();
        sequence.push_back(0);
        s4.launch();
    };

    shared_mutex mtx;
    std::vector<int> sequence;
    join(main(mtx, sequence));
    REQUIRE(sequence == std::vector{ 0, 1, 2, -1, 3, 0, 4 });
}