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


std::pmr::polymorphic_allocator<>& get_new_delete_alloc() {
    static auto alloc = std::pmr::polymorphic_allocator<>(std::pmr::new_delete_resource());
    return alloc;
}


std::pmr::polymorphic_allocator<>& get_unsynchronized_pool_alloc() {
    static auto resource = std::pmr::unsynchronized_pool_resource(std::pmr::new_delete_resource());
    static auto alloc = std::pmr::polymorphic_allocator<>(&resource);
    return alloc;
}


std::pmr::polymorphic_allocator<>& get_stack_alloc(bool renew) {
    static std::vector<std::byte> initial_buffer(1048576);
    static auto resource = std::pmr::monotonic_buffer_resource(initial_buffer.data(), initial_buffer.size(), std::pmr::new_delete_resource());
    static auto alloc = std::pmr::polymorphic_allocator<>(&resource);
    if (renew) {
        alloc.~polymorphic_allocator();
        resource.~monotonic_buffer_resource();
        new (&resource) std::pmr::monotonic_buffer_resource(initial_buffer.data(), initial_buffer.size(), std::pmr::new_delete_resource());
        new (&alloc) std::pmr::polymorphic_allocator<>(&resource);
    }
    return alloc;
}


BASELINE(task_spawn, unoptimized, 60, 50000) {
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


BENCHMARK(task_spawn, HALO, 60, 50000) {
    bool ready = false;
    {
        auto task = plain();
        task.launch();
        ready = task.ready();
    }
    assert(ready);
    celero::DoNotOptimizeAway(ready);
}


BENCHMARK(task_spawn, PMR_new_delete, 60, 50000) {
    bool ready = false;
    auto& alloc = get_new_delete_alloc();
    {
        auto task = allocator_backed(std::allocator_arg, alloc);
        volatile auto ptr = &task;
        ptr->launch();
        ready = ptr->ready();
    }
    assert(ready);
    celero::DoNotOptimizeAway(ready);
}


BENCHMARK(task_spawn, PMR_unsync_pool, 60, 50000) {
    bool ready = false;
    auto& alloc = get_unsynchronized_pool_alloc();
    {
        auto task = allocator_backed(std::allocator_arg, alloc);
        volatile auto ptr = &task;
        ptr->launch();
        ready = ptr->ready();
    }
    assert(ready);
    celero::DoNotOptimizeAway(ready);
}


BENCHMARK(task_spawn, PMR_stack, 60, 50000) {
    bool ready = false;
    static int counter = 0;
    counter = (counter + 1) % 512;
    auto& alloc = get_stack_alloc(counter == 0);
    {
        auto task = allocator_backed(std::allocator_arg, alloc);
        volatile auto ptr = &task;
        ptr->launch();
        ready = ready && ptr->ready();
    }
    assert(ready);
    celero::DoNotOptimizeAway(ready);
}