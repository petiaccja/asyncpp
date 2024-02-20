#include "helper_schedulers.hpp"

#include <asyncpp/join.hpp>
#include <asyncpp/task.hpp>
#include <asyncpp/testing/interleaver.hpp>
#include <asyncpp/thread_pool.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>

#include <catch2/catch_test_macros.hpp>


using namespace asyncpp;


struct test_promise : schedulable_promise {
    std::coroutine_handle<> handle() override {
        ++num_queried;
        return std::noop_coroutine();
    }
    std::atomic_size_t num_queried = 0;
};


TEST_CASE("Thread pool: schedule worklist selection", "[Thread pool]") {
    std::condition_variable global_notification;
    std::mutex global_mutex;
    atomic_stack<schedulable_promise, &schedulable_promise::m_scheduler_next> global_worklist;
    std::vector<thread_pool::worker> workers(1);

    test_promise promise;

    SECTION("has local worker") {
        thread_pool::schedule(promise, global_worklist, global_notification, global_mutex, &workers[0]);
        REQUIRE(workers[0].worklist.pop() == &promise);
        REQUIRE(global_worklist.empty());
    }
    SECTION("no local worker") {
        thread_pool::schedule(promise, global_worklist, global_notification, global_mutex, &workers[0]);
        REQUIRE(workers[0].worklist.pop() == &promise);
    }
}


TEST_CASE("Thread pool: steal from workers", "[Thread pool]") {
    std::vector<thread_pool::worker> workers(4);

    test_promise promise;

    SECTION("no work items") {
        REQUIRE(nullptr == thread_pool::steal(workers));
    }
    SECTION("1 work item") {
        workers[2].worklist.push(&promise);
        REQUIRE(&promise == thread_pool::steal(workers));
    }
}


TEST_CASE("Thread pool: ensure execution", "[Thread pool]") {
    // This test makes sure that no matter the interleaving, a scheduled promise
    // will be picked up and executed by a worker thread.

    struct scenario : testing::validated_scenario {
        std::condition_variable global_notification;
        std::mutex global_mutex;
        atomic_stack<schedulable_promise, &schedulable_promise::m_scheduler_next> global_worklist;
        std::vector<thread_pool::worker> workers;
        std::atomic_flag terminate;
        test_promise promise;

        scenario() : workers(1) {}

        void schedule() {
            thread_pool::schedule(promise, global_worklist, global_notification, global_mutex);
            INTERLEAVED(terminate.test_and_set());
            global_notification.notify_all();
        }

        void execute() {
            thread_pool::execute(workers[0], global_worklist, global_notification, global_mutex, terminate, std::span(workers));
        }

        void validate(const testing::path& p) override {
            INFO(p.dump());
            REQUIRE(promise.num_queried.load() > 0);
        }
    };

    INTERLEAVED_RUN(scenario, THREAD("schedule", &scenario::schedule), THREAD("execute", &scenario::execute));
}


constexpr size_t num_threads = 4;
constexpr int64_t depth = 5;
constexpr size_t branching = 10;


TEST_CASE("Thread pool: smoke test - schedule tasks", "[Scheduler]") {
    thread_pool sched(num_threads);

    static const auto coro = [&sched](auto self, int depth) -> task<int64_t> {
        if (depth <= 0) {
            co_return 1;
        }
        std::array<task<int64_t>, branching> children;
        std::ranges::generate(children, [&] { return launch(self(self, depth - 1), sched); });
        int64_t sum = 0;
        for (auto& tk : children) {
            sum += co_await tk;
        }
        co_return sum;
    };

    const auto count = int64_t(std::pow(branching, depth));
    const auto result = join(bind(coro(coro, depth), sched));
    REQUIRE(result == count);
}