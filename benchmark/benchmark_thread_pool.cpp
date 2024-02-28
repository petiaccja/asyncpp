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


template <int NumThreads>
struct FixturePool : celero::TestFixture {
    FixturePool() : pool(NumThreads), num_running(0), promises(400000, noop_promise(&num_running)) {}

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

    thread_pool_3 pool;
    std::atomic_size_t num_running;
    std::vector<noop_promise> promises;
};


BASELINE_F(tp_schedule_outside, x1_thread, FixturePool<1>, 30, 1) {
    for (auto& promise : promises) {
        pool.schedule(promise);
    }
}


BENCHMARK_F(tp_schedule_outside, x2_thread, FixturePool<4>, 30, 1) {
    for (auto& promise : promises) {
        pool.schedule(promise);
    }
}


BENCHMARK_F(tp_schedule_outside, x4_thread, FixturePool<4>, 30, 1) {
    for (auto& promise : promises) {
        pool.schedule(promise);
    }
}


BASELINE_F(tp_schedule_inside, x1_thread, FixturePool<1>, 30, 1) {
    static constexpr auto coro = [](std::span<noop_promise> promises, thread_pool_3& pool) -> task<void> {
        for (auto& promise : promises) {
            pool.schedule(promise);
        }
        co_return;
    };
    auto t1 = launch(coro(promises, pool), pool);
    join(t1);
}


BENCHMARK_F(tp_schedule_inside, x2_thread, FixturePool<2>, 30, 1) {
    static constexpr auto coro = [](std::span<noop_promise> promises, thread_pool_3& pool) -> task<void> {
        for (auto& promise : promises) {
            pool.schedule(promise);
        }
        co_return;
    };
    const auto count = std::ssize(promises) / 2;
    auto t1 = launch(coro(std::span(promises.begin(), promises.begin() + count), pool), pool);
    auto t2 = launch(coro(std::span(promises.begin() + count, promises.end()), pool), pool);
    join(t1);
    join(t2);
}


BENCHMARK_F(tp_schedule_inside, x4_thread, FixturePool<4>, 30, 1) {
    static constexpr auto coro = [](std::span<noop_promise> promises, thread_pool_3& pool) -> task<void> {
        for (auto& promise : promises) {
            pool.schedule(promise);
        }
        co_return;
    };
    const auto count = std::ssize(promises) / 4;
    auto t1 = launch(coro(std::span(promises.begin(), promises.begin() + 1 * count), pool), pool);
    auto t2 = launch(coro(std::span(promises.begin() + 1 * count, promises.begin() + 2 * count), pool), pool);
    auto t3 = launch(coro(std::span(promises.begin() + 2 * count, promises.begin() + 3 * count), pool), pool);
    auto t4 = launch(coro(std::span(promises.begin() + 3 * count, promises.end()), pool), pool);
    join(t1);
    join(t2);
    join(t3);
    join(t4);
}


BASELINE_F(tp_stealing, x1_thread, FixturePool<1 + 1>, 30, 1) {
    static constexpr auto coro = [](std::span<noop_promise> promises, thread_pool_3& pool) -> task<void> {
        for (auto& promise : promises) {
            pool.schedule(promise);
        }
        co_return;
    };
    join(launch(coro(promises, pool), pool));
}


BENCHMARK_F(tp_stealing, x2_thread, FixturePool<2 + 1>, 30, 1) {
    static constexpr auto coro = [](std::span<noop_promise> promises, thread_pool_3& pool) -> task<void> {
        for (auto& promise : promises) {
            pool.schedule(promise);
        }
        co_return;
    };
    join(launch(coro(promises, pool), pool));
}


BENCHMARK_F(tp_stealing, x4_thread, FixturePool<4 + 1>, 30, 1) {
    static constexpr auto coro = [](std::span<noop_promise> promises, thread_pool_3& pool) -> task<void> {
        for (auto& promise : promises) {
            pool.schedule(promise);
        }
        co_return;
    };
    join(launch(coro(promises, pool), pool));
}