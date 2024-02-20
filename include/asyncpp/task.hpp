#pragma once

#include "event.hpp"
#include "memory/rc_ptr.hpp"
#include "promise.hpp"
#include "scheduler.hpp"
#include "testing/suspension_point.hpp"

#include <atomic>
#include <cassert>
#include <coroutine>
#include <utility>


namespace asyncpp {


template <class T>
class task;


namespace impl_task {

    template <class T>
    struct promise : result_promise<T>, resumable_promise, schedulable_promise, impl::leak_checked_promise, rc_from_this {
        struct final_awaitable {
            constexpr bool await_ready() const noexcept { return false; }
            void await_suspend(std::coroutine_handle<promise> handle) const noexcept {
                auto& owner = handle.promise();
                owner.m_event.set(owner.m_result);
                auto self = std::move(owner.m_self); // owner.m_self.reset() call method on owner after it's been deleted.
                self.reset();
            }
            constexpr void await_resume() const noexcept {}
        };

        auto get_return_object() {
            return task<T>(rc_ptr(this));
        }

        constexpr auto initial_suspend() noexcept {
            return std::suspend_always{};
        }

        auto final_suspend() noexcept {
            return final_awaitable{};
        }

        auto handle() -> std::coroutine_handle<> final {
            return std::coroutine_handle<promise>::from_promise(*this);
        }

        void resume() final {
            return m_scheduler ? m_scheduler->schedule(*this) : handle().resume();
        }

        void start() noexcept {
            if (!INTERLEAVED(m_started.test_and_set(std::memory_order_relaxed))) {
                m_self.reset(this);
                resume();
            }
        }

        static auto await(rc_ptr<promise> pr);

        bool ready() const {
            return m_event.ready();
        }

        void destroy() {
            handle().destroy();
        }

    private:
        std::atomic_flag m_started;
        event<T> m_event;
        rc_ptr<promise> m_self;
    };

    template <class T>
    struct awaitable : event<T>::awaitable {
        using base = typename event<T>::awaitable;

        rc_ptr<promise<T>> m_awaited = nullptr;

        awaitable(base base, rc_ptr<promise<T>> awaited) : event<T>::awaitable(std::move(base)), m_awaited(awaited) {
            assert(m_awaited);
        }
    };

    template <class T>
    auto promise<T>::await(rc_ptr<promise> pr) {
        assert(pr);
        pr->start();
        auto base = pr->m_event.operator co_await();
        return awaitable<T>{ std::move(base), std::move(pr) };
    }

} // namespace impl_task


template <class T>
class [[nodiscard]] task {
public:
    using promise_type = impl_task::promise<T>;

    task() = default;
    task(const task& rhs) = delete;
    task& operator=(const task& rhs) = delete;
    task(task&& rhs) noexcept = default;
    task& operator=(task&& rhs) noexcept = default;
    task(rc_ptr<promise_type> promise) : m_promise(std::move(promise)) {}

    bool valid() const {
        return !!m_promise;
    }

    bool ready() const {
        assert(valid());
        return m_promise->ready();
    }

    void launch() {
        assert(valid());
        m_promise->start();
    }

    void bind(scheduler& scheduler) {
        assert(valid());
        if (m_promise) {
            m_promise->m_scheduler = &scheduler;
        }
    }

    auto operator co_await() {
        assert(valid());
        return promise_type::await(std::move(m_promise));
    }

private:
    rc_ptr<promise_type> m_promise;
};


} // namespace asyncpp