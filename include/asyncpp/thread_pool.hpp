#pragma once


#include "container/atomic_stack.hpp"
#include "scheduler.hpp"

#include <atomic>
#include <condition_variable>
#include <span>
#include <thread>
#include <vector>


namespace asyncpp {


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