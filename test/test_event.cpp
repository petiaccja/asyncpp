#include <async++/event.hpp>
#include <async++/interleaving/runner.hpp>
#include <async++/task.hpp>

#include <functional>

#include <catch2/catch_test_macros.hpp>

using namespace asyncpp;


TEST_CASE("Event: co_await interleaving", "[Event]") {
    struct fixture {
        event<int> evt;
    };

    auto make_fixture = [] {
        return std::make_shared<fixture>();
    };

    auto wait_thread = [](std::shared_ptr<fixture> f) {
        const auto coro = [f]() -> task<int> {
            co_return co_await f->evt;
        };
        auto t = coro();
        t.launch();
    };
    auto set_thread = [](std::shared_ptr<fixture> f) {
        f->evt.set_value(3);
    };

    auto gen = interleaving::run_all(std::function(make_fixture),
                                     std::vector{ std::function(wait_thread), std::function(set_thread) },
                                     { "$wait", "$set" });
    size_t count = 0;
    for ([[maybe_unused]] const auto& il : gen) {
        ++count;
        INFO((interleaving::interleaving_printer{ il, true }));
        std::cout << (interleaving::interleaving_printer{ il, false }) << std::endl;
    }
    REQUIRE(count >= 3);
}