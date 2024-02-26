#include <asyncpp/join.hpp>
#include <asyncpp/task.hpp>

#include <array>
#include <memory_resource>

#include <celero/Celero.h>


using namespace asyncpp;


task<int> plain() {
    co_return 1;
}


task<int> allocator_backed(std::allocator_arg_t, std::pmr::polymorphic_allocator<> alloc) {
    co_return 1;
}


struct FixtureNewDelete : celero::TestFixture {
    inline std::pmr::polymorphic_allocator<>& getAlloc() {
        return alloc;
    }

private:
    std::pmr::polymorphic_allocator<> alloc = { std::pmr::new_delete_resource() };
};


struct FixturePool : celero::TestFixture {
    inline std::pmr::polymorphic_allocator<>& getAlloc() {
        return alloc;
    }

private:
    std::pmr::unsynchronized_pool_resource resource;
    std::pmr::polymorphic_allocator<> alloc = { &resource };
};


struct FixtureStack : celero::TestFixture {
    void setUp(const ExperimentValue* x) override {
        alloc.~polymorphic_allocator();
        resource.~monotonic_buffer_resource();
        new (&resource) std::pmr::monotonic_buffer_resource(buffer.get(), size, std::pmr::new_delete_resource());
        new (&alloc) std::pmr::polymorphic_allocator<>(&resource);
    }

    inline std::pmr::polymorphic_allocator<>& getAlloc() {
        return alloc;
    }

private:
    static constexpr inline size_t size = 10485760;
    struct block {
        alignas(64) std::byte content[64];
    };
    std::unique_ptr<block[]> buffer = std::make_unique_for_overwrite<block[]>(size / sizeof(block));
    std::pmr::monotonic_buffer_resource resource;
    std::pmr::polymorphic_allocator<> alloc = { &resource };
};


constexpr int numSamples = 1000;
constexpr int numIterations = 5000;


BASELINE(task_spawn, unoptimized, numSamples, numIterations) {
    bool ready = false;
    {
        auto task = plain();
        volatile auto ptr = &task;
        ptr->launch();
        ready = ptr->ready();
    }
    assert(ready);
    celero::DoNotOptimizeAway(ready);
}


BENCHMARK(task_spawn, HALO, numSamples, numIterations) {
    bool ready = false;
    {
        auto task = plain();
        task.launch();
        ready = task.ready();
    }
    assert(ready);
    celero::DoNotOptimizeAway(ready);
}


BENCHMARK_F(task_spawn, PMR_new_delete, FixtureNewDelete, numSamples, numIterations) {
    bool ready = false;
    auto& alloc = getAlloc();
    {
        auto task = allocator_backed(std::allocator_arg, alloc);
        volatile auto ptr = &task;
        ptr->launch();
        ready = ptr->ready();
    }
    assert(ready);
    celero::DoNotOptimizeAway(ready);
}


BENCHMARK_F(task_spawn, PMR_unsync_pool, FixturePool, numSamples, numIterations) {
    bool ready = false;
    auto& alloc = getAlloc();
    {
        auto task = allocator_backed(std::allocator_arg, alloc);
        volatile auto ptr = &task;
        ptr->launch();
        ready = ptr->ready();
    }
    assert(ready);
    celero::DoNotOptimizeAway(ready);
}


BENCHMARK_F(task_spawn, PMR_stack, FixtureStack, numSamples, numIterations) {
    bool ready = false;
    auto& alloc = getAlloc();
    {
        auto task = allocator_backed(std::allocator_arg, alloc);
        volatile auto ptr = &task;
        ptr->launch();
        ready = ptr->ready();
    }
    assert(ready);
    celero::DoNotOptimizeAway(ready);
}