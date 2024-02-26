#include <asyncpp/join.hpp>
#include <asyncpp/task.hpp>
#include <asyncpp/thread_pool.hpp>

#include <atomic>
#include <span>

#include <celero/Celero.h>


using namespace asyncpp;


struct noop_promise : schedulable_promise {
    noop_promise(std::atomic_size_t* counter = nullptr) : counter(counter) {}
    std::coroutine_handle<> handle() override {
        if (counter) {
            counter->fetch_sub(1, std::memory_order_relaxed);
        }
        return std::noop_coroutine();
    }
    std::atomic_size_t* counter = nullptr;
};


struct FixtureScheduleOffthread : celero::TestFixture {
    FixtureScheduleOffthread() : pool(1), num_running(0), promises(400000, noop_promise(&num_running)) {}

    void setUp(const ExperimentValue* x) override {
        num_running.store(promises.size());
    }

    void tearDown() override {
        while (num_running.load(std::memory_order_relaxed) != 0) {
        }
    }

    std::atomic_size_t num_running;
    thread_pool pool;
    std::vector<noop_promise> promises;
};


BASELINE_F(thread_pool_schedule, off_thread, FixtureScheduleOffthread, 30, 1) {
    for (auto& promise : promises) {
        pool.schedule(promise);
    }
}


BENCHMARK_F(thread_pool_schedule, pool_thread, FixtureScheduleOffthread, 30, 1) {
    static constexpr auto coro = [](std::span<noop_promise> promises, thread_pool& pool) -> task<void> {
        for (auto& promise : promises) {
            pool.schedule(promise);
        }
        co_return;
    };
    join(launch(coro(promises, pool), pool));
}


template <int NumThreads>
struct FixtureStealing : celero::TestFixture {
    FixtureStealing() : pool(NumThreads + 1), num_running(0), promises(400000, noop_promise(&num_running)) {}

    void setUp(const ExperimentValue* x) override {
        num_running.store(promises.size());
    }


    void sync() {
        while (num_running.load(std::memory_order_relaxed) != 0) {
        }
    }

    void tearDown() override {
        sync();
    }

    thread_pool pool;
    std::atomic_size_t num_running;
    std::vector<noop_promise> promises;
};


BASELINE_F(thread_pool_stealing, x1_thread, FixtureStealing<1>, 30, 1) {
    static constexpr auto coro = [](std::span<noop_promise> promises, thread_pool& pool) -> task<void> {
        for (auto& promise : promises) {
            pool.schedule(promise);
        }
        co_return;
    };
    join(launch(coro(promises, pool), pool));
}


BENCHMARK_F(thread_pool_stealing, x2_thread, FixtureStealing<2>, 30, 1) {
    static constexpr auto coro = [](std::span<noop_promise> promises, thread_pool& pool) -> task<void> {
        for (auto& promise : promises) {
            pool.schedule(promise);
        }
        co_return;
    };
    join(launch(coro(promises, pool), pool));
}


BENCHMARK_F(thread_pool_stealing, x4_thread, FixtureStealing<4>, 30, 1) {
    static constexpr auto coro = [](std::span<noop_promise> promises, thread_pool& pool) -> task<void> {
        for (auto& promise : promises) {
            pool.schedule(promise);
        }
        co_return;
    };
    join(launch(coro(promises, pool), pool));
}
