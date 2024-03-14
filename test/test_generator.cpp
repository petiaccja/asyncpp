#include <asyncpp/generator.hpp>

#include <vector>

#include <catch2/catch_test_macros.hpp>


using namespace asyncpp;


TEST_CASE("Generator: sequence of values", "[Generator]") {
    static const auto coro = [](int count) -> generator<int> {
        for (int i = 0; i < count; ++i) {
            co_yield i;
        }
    };
    const auto g = coro(4);
    std::vector<int> values{ begin(g), end(g) };
    REQUIRE(values == std::vector{ 0, 1, 2, 3 });
}


TEST_CASE("Generator: sequence of references", "[Generator]") {
    static const auto coro = [](int count) -> generator<int&> {
        static int i;
        for (i = 0; i < count; ++i) {
            co_yield i;
        }
    };
    const auto g = coro(8);
    std::vector<int> values;
    for (auto& item : g) {
        values.push_back(item++);
    }
    REQUIRE(values == std::vector{ 0, 2, 4, 6 });
}


TEST_CASE("Generator: sequence of mvoeables", "[Generator]") {
    static const auto coro = [](int count) -> generator<std::unique_ptr<int>> {
        for (int i = 0; i < count; ++i) {
            co_yield std::make_unique<int>(i);
        }
    };
    const auto g = coro(4);
    std::vector<int> values;
    for (auto& item : g) {
        values.push_back(*item);
    }
    REQUIRE(values == std::vector{ 0, 1, 2, 3 });
}