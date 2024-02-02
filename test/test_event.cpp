#include "helper_interleaving.hpp"

#include <asyncpp/event.hpp>
#include <asyncpp/interleaving/runner.hpp>
#include <asyncpp/task.hpp>

#include <functional>

#include <catch2/catch_test_macros.hpp>

using namespace asyncpp;


TEST_CASE("Event: interleave co_await | set", "[Event]") {
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
    evaluate_interleavings(std::move(gen));
}
