#include <async++/join.hpp>
#include <async++/task.hpp>

#include <catch2/catch_test_macros.hpp>


using namespace asyncpp;


static_assert(std::is_same_v<await_result_t<task<void>>, void>);
static_assert(std::is_same_v<await_result_t<task<int>>, int>);
static_assert(std::is_same_v<await_result_t<task<int&>>, int&>);


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
    int& result = join(t);
    REQUIRE(result == 1);
    REQUIRE(&result == &value);
}