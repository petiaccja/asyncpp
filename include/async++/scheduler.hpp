#pragma once


#include "atomic_list.hpp"
#include "schedulable.hpp"

#include <atomic>
#include <condition_variable>
#include <thread>
#include <vector>


namespace asyncpp {

class scheduler {
public:
    virtual ~scheduler() = default;
    virtual void schedule(impl::schedulable_promise& promise) = 0;
};


class thread_pool : public scheduler {
    struct worker_state {
        atomic_list<impl::schedulable_promise, &impl::schedulable_promise::m_scheduler_next> m_work_items;
        std::condition_variable m_notifier;
        std::mutex m_notification_mutex;
        worker_state* m_next;
        const thread_pool* m_owner;
    };

public:
    thread_pool(size_t num_threads = std::thread::hardware_concurrency());
    thread_pool(thread_pool&&) = delete;
    thread_pool& operator=(thread_pool&&) = delete;
    ~thread_pool() noexcept;
    void schedule(impl::schedulable_promise& promise) noexcept override;

private:
    void worker_function() noexcept;
    void sync_worker_init() noexcept;

private:
    static thread_local worker_state m_worker_state;
    std::vector<std::thread> m_worker_threads;
    std::vector<std::atomic<worker_state*>> m_worker_states;
    std::atomic_flag m_finished;
    std::atomic_size_t m_worker_index = 0;
    atomic_list<worker_state, &worker_state::m_next> m_free_workers;
};


template <class T>
    requires requires(T& t, scheduler& s) { t.set_scheduler(s); }
T& set_scheduler(T& t, scheduler& s) {
    t.set_scheduler(s);
    return t;
}


template <class T>
    requires requires(T&& t, scheduler& s) { t.set_scheduler(s); }
T set_scheduler(T&& t, scheduler& s) {
    t.set_scheduler(s);
    return std::move(t);
}


} // namespace asyncpp