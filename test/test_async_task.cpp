#include "leak_tester.hpp"
#include "test_schedulers.hpp"

#include <async++/async_task.hpp>
#include <async++/interleaving/runner.hpp>

#include <catch2/catch_test_macros.hpp>


using namespace asyncpp;


TEST_CASE("Async task: interleaving sync", "[Async task]") {
    static const auto do_task = [](leak_tester value) -> async_task<int> { co_return 3; };

    struct fixture {
        leak_tester tester;
        thread_locked_scheduler sched;
        async_task<int> task;
    };

    leak_tester tester;

    auto make_fixture = [&tester] {
        auto f = std::make_shared<fixture>(tester);
        f->task = do_task(tester);
        set_scheduler(f->task, f->sched);
        return f;
    };

    auto sync_thread = [](std::shared_ptr<fixture> f) {
        f->task.launch();
        f->task.get();
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
    REQUIRE(count > 0);
}


TEST_CASE("Async task: interleaving co_await", "[Async task]") {
    static const auto do_worker_task = [](leak_tester tester) -> async_task<int> {
        co_return 3;
    };
    static const auto do_main_task = [](async_task<int> task) -> async_task<int> {
        task.launch();
        co_return co_await task;
    };

    struct fixture {
        leak_tester tester;
        thread_locked_scheduler worker_sched;
        thread_locked_scheduler main_sched;
        async_task<int> main_task;
    };

    leak_tester tester;

    auto make_fixture = [&tester] {
        auto f = std::make_shared<fixture>(tester);
        auto worker_task = do_worker_task(tester);
        set_scheduler(worker_task, f->worker_sched);
        f->main_task = do_main_task(std::move(worker_task));
        f->main_task.set_scheduler(f->main_sched);
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
    REQUIRE(count > 0);
}


TEST_CASE("Async task: interleaving abandon", "[Async task]") {
    static const auto do_task = [](leak_tester value) -> async_task<int> { co_return 3; };

    struct fixture {
        leak_tester tester;
        thread_locked_scheduler sched;
        async_task<int> task;
    };

    leak_tester tester;

    auto make_fixture = [&tester] {
        auto f = std::make_shared<fixture>(tester);
        f->task = do_task(tester);
        set_scheduler(f->task, f->sched);
        return f;
    };

    auto sync_thread = [](std::shared_ptr<fixture> f) {
        f->task.launch();
        f->task = {};
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
    REQUIRE(count > 0);
}


TEST_CASE("Async task: get value", "[Async task]") {
    static const auto coro = [](int value) -> async_task<int> {
        co_return value;
    };
    REQUIRE(coro(42).get() == 42);
}


TEST_CASE("Async task: co_await value", "[Async task]") {
    static const auto coro = [](int value) -> async_task<int> {
        co_return value;
    };
    static const auto enclosing = [](int value) -> async_task<int> {
        co_return co_await coro(value);
    };
    REQUIRE(enclosing(42).get() == 42);
}


TEST_CASE("Async task: get ref", "[Async task]") {
    static const auto coro = [](int value) -> async_task<int> {
        co_return value;
    };
    REQUIRE(coro(42).get() == 42);
}


TEST_CASE("Async task: co_await ref", "[Async task]") {
    static int value = 42;
    static const auto coro = [](int& value) -> async_task<int&> {
        co_return value;
    };
    static const auto enclosing = [](int& value) -> async_task<int&> {
        co_return co_await coro(value);
    };
    auto task = enclosing(value);
    auto& result = task.get();
    REQUIRE(result == 42);
    REQUIRE(&result == &value);
}