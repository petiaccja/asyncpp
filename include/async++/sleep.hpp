#pragma once

#include "awaitable.hpp"
#include "promise.hpp"

#include <chrono>
#include <concepts>
#include <coroutine>


namespace asyncpp {

namespace impl_sleep {

    using clock_type = std::chrono::steady_clock;

    struct awaitable : basic_awaitable<void> {
        awaitable(clock_type::time_point time) noexcept;

        bool await_ready() const noexcept;
        template <std::convertible_to<const resumable_promise&> Promise>
        void await_suspend(std::coroutine_handle<Promise> enclosing) noexcept;
        void await_resume() const noexcept;
        void on_ready() noexcept override;
        auto get_time() const noexcept -> clock_type::time_point;

    private:
        void enqueue() noexcept;

        clock_type::time_point m_time;
        resumable_promise* m_enclosing = nullptr;
    };

    template <std::convertible_to<const resumable_promise&> Promise>
    void awaitable::await_suspend(std::coroutine_handle<Promise> enclosing) noexcept {
        m_enclosing = &enclosing.promise();
        enqueue();
    }

} // namespace impl_sleep


template <class Rep, class Period>
auto sleep_for(std::chrono::duration<Rep, Period> duration) -> impl_sleep::awaitable {
    using impl_sleep::clock_type;
    return { clock_type::now() + duration };
}


template <class Clock, class Dur>
auto sleep_until(std::chrono::time_point<Clock, Dur> time_point) -> impl_sleep::awaitable {
    using impl_sleep::clock_type;
    return { std::chrono::clock_cast<clock_type>(time_point) };
}

} // namespace asyncpp