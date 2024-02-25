#pragma once

#include "event.hpp"
#include "memory/rc_ptr.hpp"
#include "promise.hpp"
#include "scheduler.hpp"
#include "testing/suspension_point.hpp"

#include <cassert>


namespace asyncpp {


namespace impl_task {

    template <class T, class Task, class Event>
    struct promise : result_promise<T>, resumable_promise, schedulable_promise, rc_from_this {
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
            return Task(rc_ptr(this));
        }

        constexpr auto initial_suspend() const noexcept {
            return std::suspend_always{};
        }

        auto final_suspend() const noexcept {
            return final_awaitable{};
        }

        auto handle() -> std::coroutine_handle<> final {
            return std::coroutine_handle<promise>::from_promise(*this);
        }

        void resume() final {
            return m_scheduler ? m_scheduler->schedule(*this) : handle().resume();
        }

        void start() {
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
        Event m_event;
        rc_ptr<promise> m_self;
    };

    template <class T, class Promise, class Event>
    struct awaitable : Event::awaitable {
        rc_ptr<Promise> m_awaited = nullptr;

        awaitable(typename Event::awaitable base, rc_ptr<Promise> awaited)
            : Event::awaitable(std::move(base)), m_awaited(awaited) {
            assert(m_awaited);
        }
    };

    template <class T, class Task, class Event>
    auto promise<T, Task, Event>::await(rc_ptr<promise> pr) {
        assert(pr);
        pr->start();
        auto base = pr->m_event.operator co_await();
        return awaitable<T, promise, Event>{ std::move(base), std::move(pr) };
    }

} // namespace impl_task


template <class T>
class [[nodiscard]] task {
public:
    using promise_type = impl_task::promise<T, task, event<T>>;

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


template <class T>
class [[nodiscard]] shared_task {
public:
    using promise_type = impl_task::promise<T, shared_task, broadcast_event<T>>;

    shared_task() = default;
    shared_task(rc_ptr<promise_type> promise) : m_promise(std::move(promise)) {}

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
        return promise_type::await(m_promise);
    }

private:
    rc_ptr<promise_type> m_promise;
};


} // namespace asyncpp