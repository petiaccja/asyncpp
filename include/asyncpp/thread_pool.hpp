#pragma once


#include "container/atomic_deque.hpp"
#include "container/atomic_stack.hpp"
#include "scheduler.hpp"
#include "threading/cache.hpp"
#include "threading/spinlock.hpp"

#include <atomic>
#include <condition_variable>
#include <semaphore>
#include <thread>
#include <vector>


namespace asyncpp {

class thread_pool : public scheduler {
public:
    struct pack;

    class worker {
    public:
        using queue = deque<schedulable_promise, &schedulable_promise::m_scheduler_prev, &schedulable_promise::m_scheduler_next>;

        worker();
        ~worker();

        void insert(schedulable_promise& promise);
        schedulable_promise* steal_from_this();
        schedulable_promise* try_get_promise(pack& pack, size_t& stealing_attempt, bool& exit_loop);
        schedulable_promise* steal_from_other(pack& pack, size_t& stealing_attempt) const;
        void start(pack& pack);
        void cancel();

    private:
        void run(pack& pack);

    public:
        worker* m_next = nullptr;

    private:
        alignas(avoid_false_sharing) spinlock m_spinlock;
        alignas(avoid_false_sharing) queue m_promises;
        alignas(avoid_false_sharing) std::atomic_flag m_blocked;
        alignas(avoid_false_sharing) std::binary_semaphore m_sema;
        alignas(avoid_false_sharing) std::jthread m_thread;
        alignas(avoid_false_sharing) std::atomic_flag m_cancelled;
    };

    struct pack {
        alignas(avoid_false_sharing) std::vector<worker> workers;
        alignas(avoid_false_sharing) atomic_stack<worker, &worker::m_next> blocked;
        alignas(avoid_false_sharing) std::atomic_size_t num_blocked = 0;
    };


public:
    thread_pool(size_t num_threads = 1);
    void schedule(schedulable_promise& promise) override;

private:
    alignas(avoid_false_sharing) pack m_pack;
    alignas(avoid_false_sharing) std::atomic_ptrdiff_t m_next_in_schedule;
    inline static thread_local worker* m_local = nullptr;
};

} // namespace asyncpp