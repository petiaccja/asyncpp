#pragma once


#include "container/atomic_deque.hpp"
#include "container/atomic_stack.hpp"
#include "scheduler.hpp"

#include <atomic>
#include <condition_variable>
#include <semaphore>
#include <span>
#include <thread>
#include <vector>


namespace asyncpp {


class thread_pool_3 : public scheduler {
public:
    struct pack;

    class worker {
    public:
        using queue = deque<schedulable_promise, &schedulable_promise::m_scheduler_prev, &schedulable_promise::m_scheduler_next>;

        worker()
            : m_sema(0) {}

        ~worker() {
            cancel();
        }

        void insert(schedulable_promise& promise) {
            std::unique_lock lk(m_spinlock, std::defer_lock);
            INTERLEAVED_ACQUIRE(lk.lock());
            const auto previous = m_promises.push_back(&promise);
            const auto blocked = m_blocked.test(std::memory_order_relaxed);
            INTERLEAVED(lk.unlock());

            if (!previous && blocked) {
                INTERLEAVED(m_sema.release());
            }
        }

        schedulable_promise* steal_from_this() {
            std::unique_lock lk(m_spinlock, std::defer_lock);
            INTERLEAVED(lk.lock());
            return m_promises.pop_front();
        }

        schedulable_promise* try_get_promise(pack& pack, size_t& stealing_attempt, bool& exit_loop) {
            std::unique_lock lk(m_spinlock, std::defer_lock);
            INTERLEAVED_ACQUIRE(lk.lock());
            const auto promise = m_promises.front();
            if (promise) {
                m_promises.pop_front();
                return promise;
            }

            if (stealing_attempt > 0) {
                INTERLEAVED(lk.unlock());
                const auto stolen = steal_from_other(pack, stealing_attempt);
                stealing_attempt = stolen ? pack.workers.size() : stealing_attempt - 1;
                return stolen;
            }


            if (INTERLEAVED(m_cancelled.test(std::memory_order_relaxed))) {
                exit_loop = true;
            }
            else {
                INTERLEAVED(m_blocked.test_and_set(std::memory_order_relaxed));
                pack.blocked.push(this);
                pack.num_blocked.fetch_add(1, std::memory_order_relaxed);
                INTERLEAVED(lk.unlock());
                INTERLEAVED_ACQUIRE(m_sema.acquire());
                INTERLEAVED(m_blocked.clear());
                stealing_attempt = pack.workers.size();
            }
            return nullptr;
        }

        schedulable_promise* steal_from_other(pack& pack, size_t& stealing_attempt) const {
            const size_t pack_size = pack.workers.size();
            const size_t my_index = this - pack.workers.data();
            const size_t victim_index = (my_index + stealing_attempt) % pack_size;
            return pack.workers[victim_index].steal_from_this();
        }

        void start(pack& pack) {
            m_thread = std::jthread([this, &pack] {
                run(pack);
            });
        }

        void cancel() {
            std::unique_lock lk(m_spinlock, std::defer_lock);
            INTERLEAVED_ACQUIRE(lk.lock());
            INTERLEAVED(m_cancelled.test_and_set(std::memory_order_relaxed));
            const auto blocked = INTERLEAVED(m_blocked.test(std::memory_order_relaxed));
            lk.unlock();
            if (blocked) {
                INTERLEAVED(m_sema.release());
            }
        }

    private:
        void run(pack& pack) {
            m_local = this;
            size_t stealing_attempt = pack.workers.size();
            bool exit_loop = false;
            while (!exit_loop) {
                const auto promise = try_get_promise(pack, stealing_attempt, exit_loop);
                if (promise) {
                    promise->resume_now();
                }
            }
        }

    public:
        worker* m_next = nullptr;

    private:
        alignas(64) spinlock m_spinlock;
        alignas(64) queue m_promises;
        alignas(64) std::atomic_flag m_blocked;
        alignas(64) std::binary_semaphore m_sema;
        alignas(64) std::jthread m_thread;
        alignas(64) std::atomic_flag m_cancelled;
    };

    struct pack {
        alignas(64) std::vector<worker> workers;
        alignas(64) atomic_stack<worker, &worker::m_next> blocked;
        alignas(64) std::atomic_size_t num_blocked = 0;
    };


public:
    thread_pool_3(size_t num_threads = 1)
        : m_pack(std::vector<worker>(num_threads)), m_next_in_schedule(0) {
        for (auto& worker : m_pack.workers) {
            worker.start(m_pack);
        }
    }

    void schedule(schedulable_promise& promise) override {
        size_t num_blocked = m_pack.num_blocked.load(std::memory_order_relaxed);
        const auto blocked = num_blocked > 0 ? m_pack.blocked.pop() : nullptr;
        if (blocked) {
            blocked->insert(promise);
            m_pack.num_blocked.fetch_sub(1, std::memory_order_relaxed);
        }
        else if (m_local) {
            m_local->insert(promise);
        }
        else {
            const auto selected = m_next_in_schedule.fetch_add(1, std::memory_order_relaxed) % m_pack.workers.size();
            m_pack.workers[selected].insert(promise);
        }
    }

private:
    alignas(64) pack m_pack;
    alignas(64) std::atomic_ptrdiff_t m_next_in_schedule;
    inline static thread_local worker* m_local = nullptr;
};


class thread_pool : public scheduler {
public:
    struct worker {
        worker* m_next = nullptr;

        std::jthread thread;
        atomic_stack<schedulable_promise, &schedulable_promise::m_scheduler_next> worklist;
    };

    thread_pool(size_t num_threads = 1);
    thread_pool(thread_pool&&) = delete;
    thread_pool operator=(thread_pool&&) = delete;
    ~thread_pool();

    void schedule(schedulable_promise& promise) override;


    static void schedule(schedulable_promise& item,
                         atomic_stack<schedulable_promise, &schedulable_promise::m_scheduler_next>& global_worklist,
                         std::condition_variable& global_notification,
                         std::mutex& global_mutex,
                         std::atomic_size_t& num_waiting,
                         worker* local = nullptr);

    static schedulable_promise* steal(std::span<worker> workers);

    static void execute(worker& local,
                        atomic_stack<schedulable_promise, &schedulable_promise::m_scheduler_next>& global_worklist,
                        std::condition_variable& global_notification,
                        std::mutex& global_mutex,
                        std::atomic_flag& terminate,
                        std::atomic_size_t& num_waiting,
                        std::span<worker> workers);

private:
    std::condition_variable m_global_notification;
    std::mutex m_global_mutex;
    atomic_stack<schedulable_promise, &schedulable_promise::m_scheduler_next> m_global_worklist;
    std::vector<worker> m_workers;
    std::atomic_flag m_terminate;
    std::atomic_size_t m_num_waiting = 0;

    inline static thread_local worker* local = nullptr;
};


} // namespace asyncpp