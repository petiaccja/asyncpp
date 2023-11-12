#pragma once

#include <async++/promise.hpp>
#include <async++/scheduler.hpp>
#include <async++/sync/atomic_list.hpp>

#include <cassert>


class thread_locked_scheduler : public asyncpp::scheduler {
public:
    void schedule(asyncpp::impl::schedulable_promise& promise) override {
        assert(m_promise == nullptr);
        m_promise.store(&promise);
    }

    void wait_and_resume() {
        asyncpp::impl::schedulable_promise* promise;
        while (!(promise = m_promise.exchange(nullptr))) {
        }
        promise->handle().resume();
    }

private:
    std::atomic<asyncpp::impl::schedulable_promise*> m_promise = nullptr;
};


class thread_queued_scheduler : public asyncpp::scheduler {
public:
    void schedule(asyncpp::impl::schedulable_promise& promise) override {
        m_items.push(&promise);
    }

    asyncpp::impl::schedulable_promise* get() {
        return m_items.pop();
    }

private:
    asyncpp::atomic_list<asyncpp::impl::schedulable_promise, &asyncpp::impl::schedulable_promise::m_scheduler_next> m_items;
};
