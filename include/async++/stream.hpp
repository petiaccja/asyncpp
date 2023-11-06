#pragma once

#include "awaitable.hpp"
#include "schedulable.hpp"
#include "scheduler.hpp"

#include <cassert>
#include <coroutine>
#include <exception>
#include <future>
#include <optional>


namespace asyncpp {


template <class T>
class stream;


namespace impl_stream {
    using namespace impl;


    template <class T>
    struct suspend_forward_results;


    template <class T>
    struct promise : resumable_promise, schedulable_promise {
        task_result<T> m_result;
        basic_awaitable<T>* m_awaiting = nullptr;

        auto get_return_object() noexcept { return stream<T>(this); }
        constexpr auto initial_suspend() const noexcept { return std::suspend_always{}; }
        void resume() override { return m_scheduler ? m_scheduler->schedule(*this) : handle().resume(); }
        auto handle() -> std::coroutine_handle<> override { return std::coroutine_handle<promise>::from_promise(*this); }
        auto final_suspend() const noexcept { return suspend_forward_results<T>{}; }
        void unhandled_exception() noexcept { m_result = std::current_exception(); }
        auto yield_value(T value) noexcept {
            m_result = std::forward<T>(value);
            return suspend_forward_results<T>();
        }
        void return_void() noexcept {
            m_result.clear();
        }
        void set_awaiting(basic_awaitable<T>* awaiting) {
            const auto previous = std::exchange(m_awaiting, awaiting);
            assert(previous == nullptr && "only one entity should ever await this promise");
        }
    };


    template <class T>
    struct suspend_forward_results {
        using promise_type = promise<T>;

        constexpr bool await_ready() const noexcept { return false; }
        bool await_suspend(std::coroutine_handle<promise_type> handle) const noexcept {
            const auto awaiting = std::exchange(handle.promise().m_awaiting, nullptr);
            if (awaiting != nullptr) {
                awaiting->set_results(std::move(handle.promise().m_result));
            }
            return true;
        }
        constexpr void await_resume() const noexcept {}
    };


    template <class T>
    struct awaitable : basic_awaitable<T> {
        task_result<T> m_result;
        promise<T>* m_awaited = nullptr;
        resumable_promise* m_enclosing = nullptr;
        std::atomic_flag m_synchronous;
        std::atomic_bool m_suspend;

        awaitable(promise<T>* awaited) : m_awaited(awaited) {
            m_awaited->set_awaiting(this);
        }

        bool await_ready() noexcept {
            m_suspend.store(true);
            m_synchronous.test_and_set();
            m_awaited->resume();
            const bool suspend = m_suspend.load();
            if (!suspend) {
                m_synchronous.clear();
            }
            return !suspend;
        }

        template <std::convertible_to<const resumable_promise&> Promise>
        void await_suspend(std::coroutine_handle<Promise> enclosing) {
            m_enclosing = &enclosing.promise();
            m_synchronous.clear();
        }

        auto await_resume() {
            using optional_result = std::optional<typename task_result<T>::wrapper_type>;
            return m_result.has_value() ? optional_result(m_result.get_or_throw()) : std::nullopt;
        }

        void set_results(task_result<T> result) noexcept override {
            m_result = std::move(result);
            const bool synchronous = m_synchronous.test_and_set();
            if (synchronous) {
                m_suspend.store(false);
            }
            else {
                while (m_synchronous.test()) {
                }
                m_enclosing->resume();
            }
        }
    };


    template <class T>
    struct sync_awaitable : basic_awaitable<T> {
        void set_results(task_result<T> result) noexcept override { m_promise.set_value(std::move(result)); }
        std::promise<task_result<T>> m_promise;
        std::future<task_result<T>> m_future = m_promise.get_future();
    };

} // namespace impl_stream


template <class T>
class [[nodiscard]] stream {
public:
    using promise_type = impl_stream::promise<T>;

    stream() = default;
    stream(const stream&) = delete;
    stream& operator=(const stream&) = delete;
    stream(stream&& rhs) noexcept : m_promise(std::exchange(rhs.m_promise, nullptr)) {}
    stream& operator=(stream&& rhs) noexcept {
        destroy_if_owned();
        m_promise = std::exchange(rhs.m_promise, nullptr);
        return *this;
    }
    stream(promise_type* promise) : m_promise(promise) {}
    ~stream() { destroy_if_owned(); }

    auto get() const {
        using optional_result = std::optional<typename impl::task_result<T>::wrapper_type>;
        assert(good() && "stream is finished");
        impl_stream::sync_awaitable<T> awaitable;
        m_promise->set_awaiting(&awaitable);
        m_promise->resume();
        auto result = awaitable.m_future.get();
        return result.has_value() ? optional_result(result.get_or_throw()) : std::nullopt;
    }

    impl_stream::awaitable<T> operator co_await() const { return { m_promise }; }

    operator bool() const {
        return good();
    }

    bool good() const {
        return m_promise != nullptr;
    }

private:
    void destroy_if_owned() {
        if (m_promise) {
            std::coroutine_handle<promise_type>::from_promise(*m_promise).destroy();
        }
    }

private:
    promise_type* m_promise = nullptr;
};


} // namespace asyncpp