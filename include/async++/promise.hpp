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

} // namespace asyncpp