#include "helper_schedulers.hpp"

#include <asyncpp/join.hpp>
#include <asyncpp/shared_task.hpp>
#include <asyncpp/task.hpp>
#include <asyncpp/testing/interleaver.hpp>

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>


using namespace asyncpp;


TEMPLATE_TEST_CASE("Task: valid", "[Task]", task<void>, shared_task<void>) {
    SECTION("empty") {
        TestType t;
        REQUIRE(!t.valid());
    }
    SECTION("valid") {
        auto t = []() -> TestType { co_return; }();
        REQUIRE(t.valid());
    }
}


TEMPLATE_TEST_CASE("Task: launch & ready", "[Task]", task<void>, shared_task<void>) {
    auto t = []() -> TestType { co_return; }();
    REQUIRE(!t.ready());
    t.launch();
    REQUIRE(t.ready());
}


TEMPLATE_TEST_CASE("Task: bind", "[Task]", task<void>, shared_task<void>) {
    auto t = []() -> TestType { co_return; }();
    thread_locked_scheduler sched;
    t.bind(sched);
    t.launch();
    REQUIRE(!t.ready());
    sched.resume();
    REQUIRE(t.ready());
}


TEMPLATE_TEST_CASE("Task: interleaving co_await", "[Task]", task<int>, shared_task<int>) {
    struct scenario {
        thread_locked_scheduler awaiter_sched;
        thread_locked_scheduler awaited_sched;
        TestType result;

        scenario() {
            constexpr auto awaited = []() -> TestType {
                co_return 1;
            };
            constexpr auto awaiter = [](TestType awaited) -> TestType {
                co_return co_await awaited;
            };

            auto tmp = launch(awaited(), awaited_sched);
            result = launch(awaiter(std::move(tmp)), awaiter_sched);
        }

        void awaiter() {
            awaiter_sched.resume();
            if (!result.ready()) {
                INTERLEAVED_ACQUIRE(awaiter_sched.wait());
                awaiter_sched.resume();
            }
            REQUIRE(1 == join(result));
            result = {};
        }

        void awaited() {
            awaited_sched.resume();
        }
    };

    INTERLEAVED_RUN(
        scenario,
        THREAD("awaited", &scenario::awaited),
        THREAD("awaiter", &scenario::awaiter));
}


TEMPLATE_TEST_CASE("Task: interleaving abandon", "[Task]", task<int>, shared_task<int>) {
    struct scenario {
        thread_locked_scheduler sched;
        TestType result;

        scenario() {
            constexpr auto func = []() -> TestType {
                co_return 1;
            };

            result = launch(func(), sched);
        }

        void task() {
            sched.resume();
            result = {};
        }

        void abandon() {
            result = {};
        }
    };

    INTERLEAVED_RUN(
        scenario,
        THREAD("task", &scenario::task),
        THREAD("abandon", &scenario::abandon));
}


TEMPLATE_TEST_CASE("Task: abandon (not started)", "[Task]", task<void>, shared_task<void>) {
    static const auto coro = []() -> TestType {
        co_return;
    };
    const auto before = impl::leak_checked_promise::snapshot();
    static_cast<void>(coro());
    REQUIRE(impl::leak_checked_promise::check(before));
}


TEMPLATE_TEST_CASE("Task: co_await value", "[Task]", task<int>, shared_task<int>) {
    static const auto coro = [](int value) -> TestType {
        co_return value;
    };
    static const auto enclosing = [](int value) -> TestType {
        co_return co_await coro(value);
    };
    REQUIRE(join(enclosing(42)) == 42);
}


TEMPLATE_TEST_CASE("Task: co_await ref", "[Task]", task<int&>, shared_task<int&>) {
    static int value = 42;
    static const auto coro = [](int& value) -> TestType {
        co_return value;
    };
    static const auto enclosing = [](int& value) -> TestType {
        co_return co_await coro(value);
    };
    auto task = enclosing(value);
    auto& result = join(task);
    REQUIRE(result == 42);
    REQUIRE(&result == &value);
}


TEMPLATE_TEST_CASE("Task: co_await void", "[Task]", task<void>, shared_task<void>) {
    static int value = 42;
    static const auto coro = []() -> TestType {
        co_return;
    };
    static const auto enclosing = []() -> TestType {
        co_await coro();
    };
    auto task = enclosing();
    join(task);
}