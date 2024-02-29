#include <asyncpp/threading/cache.hpp>

#include <array>
#include <atomic>
#include <thread>

#include <celero/Celero.h>


// This file benchmarks atomic operations themselves, not the library.
// The measurements can be used as a baseline or target as to what is
// achievable and reasonable on the hardware.


using namespace asyncpp;


static constexpr size_t base_reps = 4'000'000;


BASELINE(atomic_rmw, x1_thread, 30, 1) {
    alignas(avoid_false_sharing) std::atomic_size_t counter = 0;
    static constexpr size_t reps = base_reps;

    static const auto func = [&counter] {
        for (size_t rep = 0; rep < reps; ++rep) {
            counter.fetch_add(1, std::memory_order_relaxed);
        }
    };

    std::array<std::jthread, 1> threads;
    std::ranges::generate(threads, [&] { return std::jthread(func); });
}


BENCHMARK(atomic_rmw, x2_thread, 30, 1) {
    alignas(avoid_false_sharing) std::atomic_size_t counter = 0;
    static constexpr size_t reps = base_reps / 2;

    static const auto func = [&counter] {
        for (size_t rep = 0; rep < reps; ++rep) {
            counter.fetch_add(1, std::memory_order_relaxed);
        }
    };

    std::array<std::jthread, 2> threads;
    std::ranges::generate(threads, [&] { return std::jthread(func); });
}


BENCHMARK(atomic_rmw, x4_thread, 30, 1) {
    alignas(avoid_false_sharing) std::atomic_size_t counter = 0;
    static constexpr size_t reps = base_reps / 4;

    static const auto func = [&counter] {
        for (size_t rep = 0; rep < reps; ++rep) {
            counter.fetch_add(1, std::memory_order_relaxed);
        }
    };

    std::array<std::jthread, 4> threads;
    std::ranges::generate(threads, [&] { return std::jthread(func); });
}


BENCHMARK(atomic_rmw, x8_thread, 30, 1) {
    alignas(avoid_false_sharing) std::atomic_size_t counter = 0;
    static constexpr size_t reps = base_reps / 8;

    static const auto func = [&counter] {
        for (size_t rep = 0; rep < reps; ++rep) {
            counter.fetch_add(1, std::memory_order_relaxed);
        }
    };

    std::array<std::jthread, 8> threads;
    std::ranges::generate(threads, [&] { return std::jthread(func); });
}


BASELINE(atomic_read, x1_thread, 30, 1) {
    alignas(avoid_false_sharing) std::atomic_size_t counter = 0;
    static constexpr size_t reps = base_reps;

    static const auto func = [&counter] {
        for (size_t rep = 0; rep < reps; ++rep) {
            static_cast<void>(counter.load(std::memory_order_relaxed));
        }
    };

    std::array<std::jthread, 1> threads;
    std::ranges::generate(threads, [&] { return std::jthread(func); });
}


BENCHMARK(atomic_read, x2_thread, 30, 1) {
    alignas(avoid_false_sharing) std::atomic_size_t counter = 0;
    static constexpr size_t reps = base_reps / 2;

    static const auto func = [&counter] {
        for (size_t rep = 0; rep < reps; ++rep) {
            static_cast<void>(counter.load(std::memory_order_relaxed));
        }
    };

    std::array<std::jthread, 2> threads;
    std::ranges::generate(threads, [&] { return std::jthread(func); });
}


BENCHMARK(atomic_read, x4_thread, 30, 1) {
    alignas(avoid_false_sharing) std::atomic_size_t counter = 0;
    static constexpr size_t reps = base_reps / 4;

    static const auto func = [&counter] {
        for (size_t rep = 0; rep < reps; ++rep) {
            static_cast<void>(counter.load(std::memory_order_relaxed));
        }
    };

    std::array<std::jthread, 4> threads;
    std::ranges::generate(threads, [&] { return std::jthread(func); });
}


BENCHMARK(atomic_read, x8_thread, 30, 1) {
    alignas(avoid_false_sharing) std::atomic_size_t counter = 0;
    static constexpr size_t reps = base_reps / 8;

    static const auto func = [&counter] {
        for (size_t rep = 0; rep < reps; ++rep) {
            static_cast<void>(counter.load(std::memory_order_relaxed));
        }
    };

    std::array<std::jthread, 8> threads;
    std::ranges::generate(threads, [&] { return std::jthread(func); });
}