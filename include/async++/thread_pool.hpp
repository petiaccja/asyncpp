#pragma once


#include "scheduler.hpp"
#include "sync/atomic_stack.hpp"

#include <atomic>
#include <condition_variable>
#include <thread>
#include <vector>


namespace asyncpp {


class thread_pool : public scheduler {
    struct worker {
        worker* m_next;

    public:
        void add_work_item(impl::schedulable_promise& promise);
        impl::schedulable_promise* get_work_item();
        bool has_work() const;
        bool is_terminated() const;
        void terminate();
        void wake() const;
        void wait() const;

    private:
        atomic_stack<impl::schedulable_promise, &impl::schedulable_promise::m_scheduler_next> m_work_items;
        mutable std::condition_variable m_wake_cv;
        mutable std::mutex m_wake_mutex;
        std::atomic_flag m_terminated;
    };

public:
    thread_pool(size_t num_threads = std::thread::hardware_concurrency());
    thread_pool(thread_pool&&) = delete;
    thread_pool& operator=(thread_pool&&) = delete;
    ~thread_pool() noexcept;

    void schedule(impl::schedulable_promise& promise) noexcept override;

private:
    void worker_function(std::shared_ptr<worker> thread) noexcept;

private:
    std::vector<std::jthread> m_os_threads;
    std::vector<std::shared_ptr<worker>> m_workers;
    atomic_stack<worker, &worker::m_next> m_free_workers;

    static thread_local std::shared_ptr<worker> local_worker;
    static thread_local thread_pool* local_owner;
};


} // namespace asyncpp