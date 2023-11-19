#include "leak_tester.hpp"
#include "test_schedulers.hpp"

#include <async++/interleaving/runner.hpp>
#include <async++/join.hpp>
#include <async++/task.hpp>

#include <catch2/catch_test_macros.hpp>


using namespace asyncpp;


TEST_CASE("Task: interleaving co_await", "[Task]") {
    static const auto do_worker_task = [](leak_tester tester) -> task<int> {
        co_return 3;
    };
    static const auto do_main_task = [](task<int> tsk) -> task<int> {
        tsk.launch();
        co_return co_await tsk;
    };

    struct fixture {
        thread_locked_scheduler worker_sched;
        thread_locked_scheduler main_sched;
        task<int> main_task;
    };

    leak_tester tester;

    auto make_fixture = [&tester] {
        auto f = std::make_shared<fixture>();
        auto worker_task = do_worker_task(tester);
        bind(worker_task, f->worker_sched);
        f->main_task = do_main_task(std::move(worker_task));
        f->main_task.bind(f->main_sched);
        return f;
    };

    auto worker_thread = [](std::shared_ptr<fixture> f) {
        INTERLEAVED_ACQUIRE(f->worker_sched.wait_and_resume());
    };
    auto main_thread = [](std::shared_ptr<fixture> f) {
        f->main_task.launch();
        INTERLEAVED_ACQUIRE(f->main_sched.wait_and_resume());
        if (!f->main_task.ready()) {
            INTERLEAVED_ACQUIRE(f->main_sched.wait_and_resume());
        }
    };

    auto gen = interleaving::run_all(std::function(make_fixture),
                                     std::vector{ std::function(worker_thread), std::function(main_thread) },
                                     { "$worker", "$main" });
    size_t count = 0;
    for ([[maybe_unused]] const auto& il : gen) {
        ++count;
        INFO((interleaving::interleaving_printer{ il, true }));
        REQUIRE(tester);
    }
    REQUIRE(count >= 3);
}


TEST_CASE("Task: interleaving abandon", "[Task]") {
    static const auto do_task = [](leak_tester value) -> task<int> { co_return 3; };

    struct fixture {
        thread_locked_scheduler sched;
        task<int> main_task;
    };

    leak_tester tester;

    auto make_fixture = [&tester] {
        auto f = std::make_shared<fixture>();
        f->main_task = do_task(tester);
        bind(f->main_task, f->sched);
        return f;
    };

    auto sync_thread = [](std::shared_ptr<fixture> f) {
        f->main_task.launch();
        f->main_task = {};
    };
    auto task_thread = [](std::shared_ptr<fixture> f) {
        INTERLEAVED_ACQUIRE(f->sched.wait_and_resume());
    };

    auto gen = interleaving::run_all(std::function(make_fixture),
                                     std::vector{ std::function(sync_thread), std::function(task_thread) },
                                     { "$sync", "$task" });
    size_t count = 0;
    for ([[maybe_unused]] const auto& il : gen) {
        ++count;
        INFO((interleaving::interleaving_printer{ il, true }));
        REQUIRE(tester);
    }
    REQUIRE(count >= 3);
}


TEST_CASE("Task: co_await value", "[Task]") {
    static const auto coro = [](int value) -> task<int> {
        co_return value;
    };
    static const auto enclosing = [](int value) -> task<int> {
        co_return co_await coro(value);
    };
    REQUIRE(join(enclosing(42)) == 42);
}


TEST_CASE("Task: co_await ref", "[Task]") {
    static int value = 42;
    static const auto coro = [](int& value) -> task<int&> {
        co_return value;
    };
    static const auto enclosing = [](int& value) -> task<int&> {
        co_return co_await coro(value);
    };
    auto task = enclosing(value);
    auto& result = join(task);
    REQUIRE(result == 42);
    REQUIRE(&result == &value);
}


TEST_CASE("Task: co_await void", "[Task]") {
    static int value = 42;
    static const auto coro = []() -> task<void> {
        co_return;
    };
    static const auto enclosing = []() -> task<void> {
        co_await coro();
    };
    auto task = enclosing();
    join(task);
}