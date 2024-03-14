#include "helper_schedulers.hpp"
#include "monitor_allocator.hpp"

#include <asyncpp/join.hpp>
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
    struct scenario : testing::validated_scenario {
        thread_locked_scheduler sched;
        TestType result;

        scenario() {
            result = launch(coro(), sched);
        }

        TestType coro() {
            co_return 1;
        };

        void task() {
            sched.resume();
        }

        void abandon() {
            result = {};
        }

        void validate(const testing::path& path) override {}
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
    static_cast<void>(coro());
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
    static const auto coro = []() -> TestType {
        co_return;
    };
    static const auto enclosing = []() -> TestType {
        co_await coro();
    };
    auto task = enclosing();
    join(task);
}


TEST_CASE("Task: co_await moveable -- task", "[Task]") {
    using TaskType = task<std::unique_ptr<int>>;
    static const auto coro = [](std::unique_ptr<int> value) -> TaskType {
        co_return value;
    };
    static const auto enclosing = [](std::unique_ptr<int> value) -> TaskType {
        co_return co_await coro(std::move(value));
    };
    auto tsk = enclosing(std::make_unique<int>(42));
    REQUIRE((*join(tsk)) == 42);
}


TEST_CASE("Task: co_await moveable -- shared_task", "[Task]") {
    using TaskType = task<std::unique_ptr<int>>;
    static const auto coro = [](std::unique_ptr<int> value) -> TaskType {
        co_return value;
    };
    static const auto enclosing = [](std::unique_ptr<int> value) -> TaskType {
        co_return std::move(co_await coro(std::move(value)));
    };
    auto tsk = enclosing(std::make_unique<int>(42));
    REQUIRE((*join(tsk)) == 42);
}


TEMPLATE_TEST_CASE("Task: co_await exception", "[Task]", task<void>, shared_task<void>) {
    static int value = 42;
    static const auto coro = []() -> TestType {
        throw std::runtime_error("test");
        co_return; // This statement is necessary!
    };
    static const auto enclosing = []() -> TestType {
        REQUIRE_THROWS_AS(co_await coro(), std::runtime_error);
    };
    auto task = enclosing();
    join(task);
}


template <class Task>
auto allocator_free(std::allocator_arg_t, monitor_allocator<>& alloc) -> Task {
    co_return alloc;
}


template <class Task>
struct allocator_object {
    auto member_coro(std::allocator_arg_t, monitor_allocator<>& alloc) -> Task {
        co_return alloc;
    }
};


TEMPLATE_TEST_CASE("Task: allocator erased", "[Task]", task<monitor_allocator<>&>, shared_task<monitor_allocator<>&>) {
    monitor_allocator<> alloc;

    SECTION("free function") {
        auto task = allocator_free<TestType>(std::allocator_arg, alloc);
        join(task);
        REQUIRE(alloc.get_num_allocations() == 1);
        REQUIRE(alloc.get_num_live_objects() == 0);
    }
    SECTION("member function") {
        allocator_object<TestType> obj;
        auto task = obj.member_coro(std::allocator_arg, alloc);
        join(task);
        REQUIRE(alloc.get_num_allocations() == 1);
        REQUIRE(alloc.get_num_live_objects() == 0);
    }
}


TEMPLATE_TEST_CASE("Task: allocator explicit", "[Task]", (task<monitor_allocator<>&, monitor_allocator<>>), (shared_task<monitor_allocator<>&, monitor_allocator<>>)) {
    monitor_allocator<> alloc;

    SECTION("free function") {
        auto task = allocator_free<TestType>(std::allocator_arg, alloc);
        join(task);
        REQUIRE(alloc.get_num_allocations() == 1);
        REQUIRE(alloc.get_num_live_objects() == 0);
    }
    SECTION("member function") {
        allocator_object<TestType> obj;
        auto task = obj.member_coro(std::allocator_arg, alloc);
        join(task);
        REQUIRE(alloc.get_num_allocations() == 1);
        REQUIRE(alloc.get_num_live_objects() == 0);
    }
}