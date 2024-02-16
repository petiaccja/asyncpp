#pragma once

#include <stdexcept>
#include <atomic>
#include <memory>
#include <asyncpp/promise.hpp>


class monitor_task {
    struct counters {
        std::atomic_size_t suspensions;
        std::atomic_bool done;
        std::exception_ptr exception;
    };

    struct promise : asyncpp::resumable_promise, asyncpp::schedulable_promise {
        monitor_task get_return_object() noexcept {
            return monitor_task{ m_counters };
        }

        constexpr std::suspend_never initial_suspend() const noexcept {
            return {};
        }

        std::suspend_always final_suspend() const noexcept {
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

        std::shared_ptr<counters> m_counters = std::make_shared<counters>();
    };

public:
    using promise_type = promise;

    monitor_task() = default;
    monitor_task(std::shared_ptr<counters> c) : m_counters(std::move(c)) {}
    monitor_task(monitor_task&&) = default;
    monitor_task(const monitor_task&) = delete;

    const counters& get_counters() const {
        return *m_counters;
    }

private:
    std::shared_ptr<counters> m_counters;
};