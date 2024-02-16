#pragma once

#include "container/atomic_collection.hpp"
#include "promise.hpp"
#include "testing/suspension_point.hpp"

#include <cassert>
#include <stdexcept>


namespace asyncpp {

template <class T>
class event {
};


template <class T>
class broadcast_event {
    struct awaitable {
        broadcast_event* m_owner = nullptr;
        resumable_promise* m_enclosing = nullptr;
        awaitable* m_next = nullptr;

        bool await_ready() const {
            assert(m_owner);
            return m_owner->m_awaiters.closed();
        }

        template <std::convertible_to<const resumable_promise&> Promise>
        bool await_suspend(std::coroutine_handle<Promise> promise) {
            assert(m_owner);
            m_enclosing = &promise.promise();
            const auto status = m_owner->m_awaiters.push(this);
            return !m_owner->m_awaiters.closed(status);
        }

        T& await_resume() {
            assert(m_owner);
            assert(m_owner->m_result.has_value());
            return m_owner->m_result.get_or_throw();
        }
    };

public:
    broadcast_event() = default;
    broadcast_event(broadcast_event&&) = delete;
    broadcast_event& operator=(broadcast_event&&) = delete;
    ~broadcast_event() {
        assert(m_awaiters.empty());
    }

    void set_value(T value) {
        if (m_result.has_value()) {
            throw std::invalid_argument("event already set");
        }
        m_result = std::move(value);
        resume_all();
    }

    void set_exception(std::exception_ptr ex) {
        if (m_result.has_value()) {
            throw std::invalid_argument("event already set");
        }
        m_result = std::move(ex);
        resume_all();
    }

    awaitable operator co_await() {
        return awaitable{ this };
    }

    task_result<T>& _debug_get_result() {
        return m_result;
    }

private:
    void resume_all() {
        auto first = m_awaiters.close();
        while (first != nullptr) {
            assert(first->m_enclosing != nullptr);
            first->m_enclosing->resume();
            first = first->m_next;
        }
    }

private:
    atomic_collection<awaitable, &awaitable::m_next> m_awaiters;
    task_result<T> m_result;
};

} // namespace asyncpp