#include <async++/join.hpp>
#include <async++/task.hpp>

#include <catch2/catch_test_macros.hpp>


using namespace asyncpp;


TEST_CASE("Join: void", "[Join]") {
    static const auto coro = []() -> task<void> {
        co_return;
    };

    auto t = coro();
    join(t);
}


TEST_CASE("Join: value", "[Join]") {
    static const auto coro = [](int value) -> task<int> {
        co_return value;
    };

    auto t = coro(1);
    REQUIRE(join(t) == 1);
}


TEST_CASE("Join: ref", "[Join]") {
    static const auto coro = [](int& value) -> task<int&> {
        co_return value;
    };

    int value = 1;
    auto t = coro(value);
    decltype(auto) result = join(t);
    REQUIRE(result == 1);
    REQUIRE(&result == &value);
}