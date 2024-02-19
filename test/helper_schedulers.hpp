#pragma once

#include <asyncpp/container/atomic_stack.hpp>
#include <asyncpp/promise.hpp>
#include <asyncpp/scheduler.hpp>

#include <cassert>


class thread_locked_scheduler : public asyncpp::scheduler {
public:
    void schedule(asyncpp::schedulable_promise& promise) override {
        assert(m_promise == nullptr);
        m_promise.store(&promise);
    }

    void wait() {
        while (nullptr == m_promise.load()) {
        }
    }

    void resume() {
        const auto promise = m_promise.exchange(nullptr);
        assert(promise != nullptr);
        promise->handle().resume();
    }

private:
    std::atomic<asyncpp::schedulable_promise*> m_promise = nullptr;
};


struct collecting_scheduler : asyncpp::scheduler {
    void schedule(asyncpp::schedulable_promise& promise) override {
        this->promise = &promise;
    }

    asyncpp::schedulable_promise* promise;
};