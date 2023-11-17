#include "leak_tester.hpp"
#include "test_schedulers.hpp"

#include <async++/interleaving/runner.hpp>
#include <async++/shared_task.hpp>

#include <catch2/catch_test_macros.hpp>


using namespace asyncpp;


TEST_CASE("Shared task: interleave sync", "[Shared task]") {
    static const auto do_task = [](leak_tester) -> shared_task<void> {
        co_return;
    };

    leak_tester tester;

    struct fixture {
        thread_locked_scheduler sched;
        shared_task<void> task;
    };

    auto make_fixture = [&tester] {
        auto f = std::make_shared<fixture>();
        f->task = do_task(tester);
        bind(f->task, f->sched);
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
                                     { "$sync_1", "$task" });

    size_t count = 0;
    for ([[maybe_unused]] const auto& il : gen) {
        ++count;
        INFO((interleaving::interleaving_printer{ il, true }));
        REQUIRE(tester);
    }
    REQUIRE(count > 0);
}


TEST_CASE("Shared task: interleaving co_await", "[Shared task]") {
    static const auto do_worker_task = [](leak_tester tester) -> shared_task<int> {
        co_return 3;
    };
    static const auto do_main_task = [](shared_task<int> task) -> shared_task<int> {
        task.launch();
        co_return co_await task;
    };

    struct fixture {
        thread_locked_scheduler worker_sched;
        thread_locked_scheduler main_sched;
        shared_task<int> main_task;
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
    REQUIRE(count > 0);
}


TEST_CASE("Shared task: interleaving abandon", "[Shared task]") {
    static const auto do_task = [](leak_tester value) -> shared_task<int> { co_return 3; };

    struct fixture {
        thread_locked_scheduler sched;
        shared_task<int> task;
    };

    leak_tester tester;

    auto make_fixture = [&tester] {
        auto f = std::make_shared<fixture>();
        f->task = do_task(tester);
        bind(f->task, f->sched);
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


TEST_CASE("Shared task: get value", "[Shared task]") {
    static const auto coro = [](int value) -> shared_task<int> {
        co_return value;
    };
    auto task = coro(1);
    REQUIRE(task.get() == 1);
    REQUIRE(task.get() == 1); // Should be possible to get a second time too.
}


TEST_CASE("Shared task: get ref", "[Shared task]") {
    static const auto coro = [](int& value) -> shared_task<int&> {
        co_return value;
    };
    int value = 1;
    auto task = coro(value);
    REQUIRE(&task.get() == &value);
}


TEST_CASE("Shared task: co_await value", "[Shared task]") {
    static const auto coro = [](int value) -> shared_task<int> {
        co_return value;
    };
    static const auto enclosing = [](int value) -> shared_task<int> {
        co_return co_await coro(value);
    };
    REQUIRE(enclosing(42).get() == 42);
}


TEST_CASE("Shared task: co_await ref", "[Shared task]") {
    static int value = 42;
    static const auto coro = [](int& value) -> shared_task<int&> {
        co_return value;
    };
    static const auto enclosing = [](int& value) -> shared_task<int&> {
        co_return co_await coro(value);
    };
    auto task = enclosing(value);
    auto& result = task.get();
    REQUIRE(result == 42);
    REQUIRE(&result == &value);
}


TEST_CASE("Shared task: co_await void", "[Shared task]") {
    static int value = 42;
    static const auto coro = []() -> shared_task<void> {
        co_return;
    };
    static const auto enclosing = []() -> shared_task<void> {
        co_await coro();
    };
    auto task = enclosing();
    task.get();
}
