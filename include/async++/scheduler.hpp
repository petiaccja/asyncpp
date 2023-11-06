#pragma once


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
    struct state {
        std::atomic<impl::schedulable_promise*> next_in_schedule = nullptr;
        std::atomic_bool is_running = true;
        std::condition_variable notifier;
        std::mutex mtx;
        std::atomic_flag pop_mtx;
    };

public:
    thread_pool(size_t num_threads = std::thread::hardware_concurrency());
    thread_pool(thread_pool&&) = default;
    thread_pool& operator=(thread_pool&&) = default;
    ~thread_pool();
    void schedule(impl::schedulable_promise& promise) override;

private:
    static void thread_function(std::shared_ptr<state> state_);
    static impl::schedulable_promise* pop(state& state_);

private:
    std::shared_ptr<state> m_state;
    std::vector<std::thread> m_threads;
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