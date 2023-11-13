#pragma once

#include "awaitable.hpp"

#include <coroutine>


namespace asyncpp {


class scheduler;


namespace impl {


    struct resumable_promise {
        virtual ~resumable_promise() = default;
        virtual void resume() = 0;
    };


    struct schedulable_promise {
        virtual ~schedulable_promise() = default;
        virtual std::coroutine_handle<> handle() = 0;
        schedulable_promise* m_scheduler_next = nullptr;
        scheduler* m_scheduler = nullptr;
    };


    template <class T>
    struct return_promise {
        task_result<T> m_result;

        void unhandled_exception() noexcept {
            m_result = std::current_exception();
        }
        void return_value(T value) noexcept {
            m_result = std::forward<T>(value);
        }
    };


    template <>
    struct return_promise<void> {
        task_result<void> m_result;

        void unhandled_exception() noexcept {
            m_result = std::current_exception();
        }
        void return_void() noexcept {
            m_result = nullptr;
        }
    };

} // namespace impl


struct resumer_coroutine_object;


struct resumer_coroutine_promise {
    impl::resumable_promise& m_promise;

    resumer_coroutine_promise(impl::resumable_promise& promise) : m_promise(promise) {}

    auto get_return_object() noexcept {
        return std::coroutine_handle<resumer_coroutine_promise>::from_promise(*this);
    }

    constexpr auto initial_suspend() const noexcept {
        return std::suspend_always{};
    }

    constexpr void return_void() const noexcept {}

    void unhandled_exception() const noexcept {
        std::terminate();
    }

    constexpr auto final_suspend() const noexcept {
        return std::suspend_never{};
    }
};


struct resumer_coroutine_handle : std::coroutine_handle<resumer_coroutine_promise> {
    resumer_coroutine_handle(std::coroutine_handle<resumer_coroutine_promise> handle)
        : std::coroutine_handle<resumer_coroutine_promise>(handle) {}
    using promise_type = resumer_coroutine_promise;
};


inline resumer_coroutine_handle resumer_coroutine(impl::resumable_promise& promise) {
    promise.resume();
    co_return;
}

} // namespace asyncpp