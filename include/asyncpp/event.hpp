#pragma once

#include "container/atomic_collection.hpp"
#include "container/atomic_item.hpp"
#include "promise.hpp"
#include "testing/suspension_point.hpp"

#include <cassert>
#include <stdexcept>


namespace asyncpp {

template <class T>
class basic_event {
    struct awaitable {
        basic_event* m_owner = nullptr;
        resumable_promise* m_enclosing = nullptr;
        awaitable* m_next = nullptr;

        bool await_ready() const {
            assert(m_owner);
            return m_owner->m_awaiter.closed();
        }

        template <std::convertible_to<const resumable_promise&> Promise>
        bool await_suspend(std::coroutine_handle<Promise> promise) {
            assert(m_owner);
            m_enclosing = &promise.promise();
            const auto status = m_owner->m_awaiter.set(this);
            if (status != nullptr && !m_owner->m_awaiter.closed(status)) {
                m_owner->m_awaiter.set(status);
                throw std::invalid_argument("event already awaited");
            }
            return !m_owner->m_awaiter.closed(status);
        }

        T await_resume() {
            assert(m_owner);
            assert(m_owner->m_result.has_value());
            return static_cast<T>(m_owner->m_result.move_or_throw());
        }
    };

public:
    basic_event() = default;
    basic_event(basic_event&&) = delete;
    basic_event& operator=(basic_event&&) = delete;
    ~basic_event() {
        assert(m_awaiter.empty());
    }

    void set_exception(std::exception_ptr ex) {
        if (m_result.has_value()) {
            throw std::invalid_argument("event already set");
        }
        m_result = std::move(ex);
        resume_one();
    }

    awaitable operator co_await() {
        return awaitable{ this };
    }

    task_result<T>& _debug_get_result() {
        return m_result;
    }

protected:
    void resume_one() {
        auto item = m_awaiter.close();
        if (item != nullptr) {
            assert(item->m_enclosing != nullptr);
            item->m_enclosing->resume();
        }
    }

protected:
    atomic_item<awaitable> m_awaiter;
    task_result<T> m_result;
};


template <class T>
class event : public basic_event<T> {
public:
    void set_value(T value) {
        if (this->m_result.has_value()) {
            throw std::invalid_argument("event already set");
        }
        this->m_result = std::forward<T>(value);
        this->resume_one();
    }
};


template <>
class event<void> : public basic_event<void> {
public:
    void set_value() {
        if (this->m_result.has_value()) {
            throw std::invalid_argument("event already set");
        }
        this->m_result = nullptr;
        this->resume_one();
    }
};


template <class T>
class basic_broadcast_event {
    struct awaitable {
        basic_broadcast_event* m_owner = nullptr;
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

        auto await_resume() -> std::conditional_t<std::is_void_v<T>, void, std::add_lvalue_reference_t<T>> {
            assert(m_owner);
            assert(m_owner->m_result.has_value());
            return m_owner->m_result.get_or_throw();
        }
    };

public:
    basic_broadcast_event() = default;
    basic_broadcast_event(basic_broadcast_event&&) = delete;
    basic_broadcast_event& operator=(basic_broadcast_event&&) = delete;
    ~basic_broadcast_event() {
        assert(m_awaiters.empty());
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

protected:
    void resume_all() {
        auto first = m_awaiters.close();
        while (first != nullptr) {
            assert(first->m_enclosing != nullptr);
            first->m_enclosing->resume();
            first = first->m_next;
        }
    }

protected:
    atomic_collection<awaitable, &awaitable::m_next> m_awaiters;
    task_result<T> m_result;
};


template <class T>
class broadcast_event : public basic_broadcast_event<T> {
public:
    void set_value(T value) {
        if (this->m_result.has_value()) {
            throw std::invalid_argument("event already set");
        }
        this->m_result = std::forward<T>(value);
        this->resume_all();
    }
};


template <>
class broadcast_event<void> : public basic_broadcast_event<void> {
public:
    void set_value() {
        if (this->m_result.has_value()) {
            throw std::invalid_argument("event already set");
        }
        this->m_result = nullptr;
        this->resume_all();
    }
};

} // namespace asyncpp