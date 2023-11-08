#pragma once

#include "awaitable.hpp"
#include "schedulable.hpp"
#include "scheduler.hpp"

#include <cassert>
#include <coroutine>
#include <future>
#include <optional>
#include <utility>


namespace asyncpp {


template <class T>
class task;


namespace impl_task {

    using namespace impl;


    template <class T>
    struct promise_result {
        task_result<T> m_result;

        void unhandled_exception() noexcept { m_result = std::current_exception(); }
        void return_value(T value) noexcept { m_result = std::forward<T>(value); }
    };


    template <>
    struct promise_result<void> {
        task_result<void> m_result;

        void unhandled_exception() noexcept { m_result = std::current_exception(); }
        void return_void() noexcept { m_result = nullptr; }
    };


    template <class T>
    struct promise : promise_result<T>, resumable_promise, schedulable_promise {
        basic_awaitable<T>* m_awaiting = nullptr;

        auto get_return_object() { return task<T>(this); }
        constexpr auto initial_suspend() const noexcept { return std::suspend_always{}; }
        void resume() override { return m_scheduler ? m_scheduler->schedule(*this) : handle().resume(); }
        auto handle() -> std::coroutine_handle<> override { return std::coroutine_handle<promise>::from_promise(*this); }
        auto final_suspend() noexcept {
            forward_results();
            return std::suspend_never{};
        }
        void set_awaiting(basic_awaitable<T>* awaiting) {
            const auto previous = std::exchange(m_awaiting, awaiting);
            assert(previous == nullptr && "only one entity should ever await this promise");
        }
        void forward_results() noexcept {
            const auto awaiting = std::exchange(m_awaiting, nullptr);
            assert(awaiting != nullptr && "lazy task must have someone awaiting it");
            awaiting->set_results(std::move(this->m_result));
        }
    };


    template <class T>
    struct awaitable : basic_awaitable<T> {
        task_result<T> m_result;
        promise<T>* m_awaited = nullptr;
        resumable_promise* m_enclosing = nullptr;

        awaitable(promise<T>* awaited) : m_awaited(awaited) { m_awaited->set_awaiting(this); }
        bool await_ready() const noexcept { return false; }
        T await_resume() { return m_result.get_or_throw(); }

        template <std::convertible_to<const resumable_promise&> Promise>
        void await_suspend(std::coroutine_handle<Promise> enclosing) {
            m_enclosing = &enclosing.promise();
            m_awaited->resume();
        }

        void set_results(task_result<T> result) noexcept override {
            m_result = std::move(result);
            m_enclosing->resume();
        }
    };


    template <class T>
    struct sync_awaitable : basic_awaitable<T> {
        task_result<T> m_result;
        promise<T>* m_awaited = nullptr;
        std::promise<task_result<T>> m_promise;
        std::future<task_result<T>> m_future = m_promise.get_future();

        sync_awaitable(promise<T>* awaited) : m_awaited(awaited) {
            m_awaited->set_awaiting(this);
            m_awaited->resume();
        }
        void set_results(task_result<T> result) noexcept override { m_promise.set_value(std::move(result)); }
    };


} // namespace impl_task


template <class T>
class [[nodiscard]] task {
public:
    using promise_type = impl_task::promise<T>;

    task() = default;
    task(const task& rhs) = delete;
    task& operator=(const task& rhs) = delete;
    task(task&& rhs) noexcept : m_promise(std::exchange(rhs.m_promise, nullptr)) {}
    task& operator=(task&& rhs) noexcept {
        destroy();
        m_promise = std::exchange(rhs.m_promise, nullptr);
        return *this;
    }
    task(promise_type* promise) : m_promise(promise) {}
    ~task() { destroy(); }

    bool valid() const {
        return m_promise != nullptr;
    }

    T get() {
        assert(valid());
        impl_task::sync_awaitable<T> awaitable(std::exchange(m_promise, nullptr));
        return awaitable.m_future.get().get_or_throw();
    }

    auto operator co_await() {
        assert(valid());
        return impl_task::awaitable<T>(std::exchange(m_promise, nullptr));
    }

    void set_scheduler(scheduler& scheduler) {
        if (m_promise) {
            m_promise->m_scheduler = &scheduler;
        }
    }

private:
    void destroy() {
        if (m_promise) {
            std::coroutine_handle<promise_type>::from_promise(*std::exchange(m_promise, nullptr)).destroy();
        }
    }

private:
    promise_type* m_promise = nullptr;
};


} // namespace asyncpp