#pragma once

#include <asyncpp/memory/rc_ptr.hpp>
#include <asyncpp/promise.hpp>
#include <asyncpp/scheduler.hpp>

#include <atomic>
#include <cassert>
#include <memory>
#include <stdexcept>


class [[nodiscard]] monitor_task {
    struct counters {
        std::atomic_size_t suspensions;
        std::atomic_bool done;
        std::exception_ptr exception;
    };

    struct promise : asyncpp::resumable_promise, asyncpp::schedulable_promise, asyncpp::rc_from_this {
        monitor_task get_return_object() noexcept {
            return monitor_task{ asyncpp::rc_ptr(this) };
        }

        constexpr std::suspend_never initial_suspend() const noexcept {
            return {};
        }

        std::suspend_always final_suspend() noexcept {
            m_counters->done.store(true);
            return {};
        }

        constexpr void return_void() const noexcept {}

        void unhandled_exception() noexcept {
            m_counters->exception = std::current_exception();
        }

        void resume() override {
            m_counters->suspensions.fetch_add(1);
            m_scheduler ? m_scheduler->schedule(*this) : handle().resume();
        }

        std::coroutine_handle<> handle() override {
            return std::coroutine_handle<promise>::from_promise(*this);
        }

        void destroy() noexcept {
            handle().destroy();
        }

        const counters& get_counters() const {
            return *m_counters;
        }

    private:
        std::shared_ptr<counters> m_counters = std::make_shared<counters>();
    };

public:
    using promise_type = promise;

    monitor_task() = default;
    monitor_task(asyncpp::rc_ptr<promise_type> promise) : m_promise(std::move(promise)) {}
    monitor_task(monitor_task&&) = default;
    monitor_task(const monitor_task&) = delete;
    monitor_task& operator=(monitor_task&&) = default;
    monitor_task& operator=(const monitor_task&) = delete;

    promise_type& promise() {
        assert(m_promise);
        return *m_promise;
    }

    std::coroutine_handle<promise_type> handle() {
        assert(m_promise);
        return std::coroutine_handle<promise_type>::from_promise(*m_promise);
    }

    const counters& get_counters() const {
        return m_promise->get_counters();
    }

private:
    asyncpp::rc_ptr<promise_type> m_promise;
};