#pragma once

#include "container/atomic_collection.hpp"
#include "interleaving/sequence_point.hpp"
#include "promise.hpp"

#include <cassert>
#include <iostream>
#include <memory>


namespace asyncpp {

namespace impl_event {
    template <class T>
    class promise {
    public:
        struct awaitable : impl::basic_awaitable<T> {
            awaitable* m_next = nullptr;

            awaitable(promise* owner) noexcept : m_owner(owner) {}

            bool await_ready() const noexcept {
                return m_owner->ready();
            }

            template <std::convertible_to<const impl::resumable_promise&> Promise>
            bool await_suspend(std::coroutine_handle<Promise> enclosing) noexcept {
                m_enclosing = &enclosing.promise();
                const bool ready = m_owner->await(this);
                return !ready;
            }

            auto await_resume() -> typename impl::task_result<T>::reference {
                return m_owner->get_result().get_or_throw();
            }

            void on_ready() noexcept final {
                m_enclosing->resume();
            }

        private:
            impl::resumable_promise* m_enclosing = nullptr;
            promise* m_owner = nullptr;
        };

    public:
        promise() = default;
        promise(promise&&) = delete;
        promise(const promise&) = delete;
        promise& operator=(promise&&) = delete;
        promise& operator=(const promise&) = delete;

        void set(impl::task_result<T> result) noexcept {
            m_result = std::move(result);
            finalize();
        }

        awaitable await() noexcept {
            return { this };
        }

        bool ready() const noexcept {
            return INTERLEAVED(m_awaiters.closed());
        }

    private:
        bool await(awaitable* awaiter) noexcept {
            const auto previous = INTERLEAVED(m_awaiters.push(awaiter));
            return m_awaiters.closed(previous);
        }

        void finalize() noexcept {
            auto awaiter = INTERLEAVED(m_awaiters.close());
            assert(!m_awaiters.closed(awaiter) && "cannot set event twice");
            while (awaiter != nullptr) {
                awaiter->on_ready();
                awaiter = awaiter->m_next;
            }
        }

        auto& get_result() noexcept {
            return this->m_result;
        }

    private:
        impl::task_result<T> m_result;
        atomic_collection<awaitable, &awaitable::m_next> m_awaiters;
    };

} // namespace impl_event


template <class T>
class event {
public:
    event() = default;
    event(const event&) = delete;
    event(event&&) = delete;
    event& operator=(const event&) = delete;
    event& operator=(event&&) = delete;

    auto operator co_await() noexcept {
        return m_promise.await();
    }

    void set_value(T value) {
        m_promise.set(std::move(value));
    }

    void set_exception(std::exception_ptr ex) {
        m_promise.set(std::move(ex));
    }

private:
    impl_event::promise<T> m_promise;
};

} // namespace asyncpp