#pragma once

#include <async++/container/atomic_stack.hpp>
#include <async++/promise.hpp>
#include <async++/scheduler.hpp>

#include <cassert>


class thread_locked_scheduler : public asyncpp::scheduler {
public:
    void schedule(asyncpp::schedulable_promise& promise) override {
        assert(m_promise == nullptr);
        m_promise.store(&promise);
    }

    void wait_and_resume() {
        asyncpp::schedulable_promise* promise;
        while (!(promise = m_promise.exchange(nullptr))) {
        }
        promise->handle().resume();
    }

private:
    std::atomic<asyncpp::schedulable_promise*> m_promise = nullptr;
};


class thread_queued_scheduler : public asyncpp::scheduler {
public:
    void schedule(asyncpp::schedulable_promise& promise) override {
        m_items.push(&promise);
    }

    asyncpp::schedulable_promise* get() {
        return m_items.pop();
    }

private:
    asyncpp::atomic_stack<asyncpp::schedulable_promise, &asyncpp::schedulable_promise::m_scheduler_next> m_items;
};
