#pragma once

#include "awaitable.hpp"
#include "promise.hpp"
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
    struct promise : return_promise<T>, resumable_promise, schedulable_promise {
        basic_awaitable<T>* m_awaiting = nullptr;

        auto get_return_object() { return task<T>(this); }
        constexpr auto initial_suspend() const noexcept { return std::suspend_always{}; }
        void resume() override { return m_scheduler ? m_scheduler->schedule(*this) : handle().resume(); }
        auto handle() -> std::coroutine_handle<> override { return std::coroutine_handle<promise>::from_promise(*this); }
        auto final_suspend() noexcept {
            m_awaiting->on_ready();
            return std::suspend_never{};
        }
        void set_awaiting(basic_awaitable<T>* awaiting) {
            const auto previous = std::exchange(m_awaiting, awaiting);
            assert(previous == nullptr && "only one entity should ever await this promise");
        }
        auto get_result() noexcept {
            return std::move(this->m_result);
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

        void on_ready() noexcept override {
            m_result = m_awaited->get_result();
            m_enclosing->resume();
        }
    };


    template <class T>
    struct sync_awaitable : basic_awaitable<T> {
        promise<T>* m_awaited = nullptr;
        std::promise<task_result<T>> m_promise;
        std::future<task_result<T>> m_future = m_promise.get_future();

        sync_awaitable(promise<T>* awaited) : m_awaited(awaited) {
            m_awaited->set_awaiting(this);
            m_awaited->resume();
        }
        void on_ready() noexcept override {
            m_promise.set_value(m_awaited->get_result());
        }
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