#pragma once

#include "concepts.hpp"
#include "promise.hpp"

#include <coroutine>
#include <exception>
#include <future>


namespace asyncpp {

namespace impl_join {

    template <class T>
    struct joiner;

    template <class T>
    struct basic_promise : impl::resumable_promise {
        std::promise<T> m_promise;

        joiner<T> get_return_object() {
            return { m_promise.get_future() };
        }

        constexpr auto initial_suspend() const noexcept {
            return std::suspend_never{};
        }

        void unhandled_exception() noexcept {
            m_promise.set_exception(std::current_exception());
        }

        constexpr auto final_suspend() const noexcept {
            return std::suspend_never{};
        }
    };

    template <class T>
    struct promise : basic_promise<T> {
        void return_value(T value) noexcept {
            this->m_promise.set_value(std::forward<T>(value));
        }

        void resume() noexcept override {
            std::coroutine_handle<promise>::from_promise(*this).resume();
        }
    };

    template <>
    struct promise<void> : basic_promise<void> {
        void return_void() noexcept {
            m_promise.set_value();
        }

        void resume() noexcept override {
            std::coroutine_handle<promise>::from_promise(*this).resume();
        }
    };

    template <class T>
    struct joiner {
        using promise_type = promise<T>;

        std::future<T> future;
    };

} // namespace impl_join


template <awaitable Awaitable>
decltype(auto) join(Awaitable&& object) {
    auto joiner_ = [&object]() -> impl_join::joiner<await_result_t<Awaitable>> {
        co_return co_await object;
    }();
    return joiner_.future.get();
}

} // namespace asyncpp