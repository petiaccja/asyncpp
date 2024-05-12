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


TEST_CASE("Generator: sequence of movables", "[Generator]") {
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


TEST_CASE("Generator: const iterators", "[Generator]") {
    static const auto coro = []() -> generator<int> {
        co_yield 0;
    };
    const auto g = coro();
    std::vector<int> values;
    for (auto it = g.cbegin(); it != g.cend(); ++it) {
        values.push_back(*it);
    }
    REQUIRE(values == std::vector{ 0 });
}


TEST_CASE("Generator: unhandled exceptions", "[Generator]") {
    static const auto coro = []() -> generator<int> {
        throw std::runtime_error("test");
        co_return;
    };
    const auto g = coro();
    REQUIRE_THROWS_AS(*g.begin(), std::runtime_error);
}


TEST_CASE("Generator: iterator++", "[Generator]") {
    static const auto coro = []() -> generator<int> {
        co_yield 0;
    };
    const auto g = coro();
    auto it = g.begin();
    it++;
    REQUIRE(it == g.end());
}


TEST_CASE("Generator: ++iterator", "[Generator]") {
    static const auto coro = []() -> generator<int> {
        co_yield 0;
    };
    const auto g = coro();
    auto it = g.begin();
    ++it;
    REQUIRE(it == g.end());
}


TEST_CASE("Generator: iterator begin", "[Generator]") {
    static const auto coro = []() -> generator<int> {
        co_return;
    };
    const auto g = coro();
    auto it = g.begin();
    REQUIRE(it == g.begin());
}


TEST_CASE("Generator: move ctor", "[Generator]") {
    static const auto coro = []() -> generator<int> {
        co_yield 0;
        co_yield 1;
    };
    auto g = coro();
    auto m = std::move(g);
    auto it = m.begin();
    REQUIRE(*it == 0);
    ++it;
    REQUIRE(*it == 1);
    ++it;
    REQUIRE(it == m.end());
}


TEST_CASE("Generator: move assign", "[Generator]") {
    static const auto coro = []() -> generator<int> {
        co_yield 0;
        co_yield 1;
    };
    auto g = coro();
    generator<int> m = coro();
    m = std::move(g);
    auto it = m.begin();
    REQUIRE(*it == 0);
    ++it;
    REQUIRE(*it == 1);
    ++it;
    REQUIRE(it == m.end());
}