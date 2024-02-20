#pragma once

#include "event.hpp"
#include "memory/rc_ptr.hpp"
#include "promise.hpp"
#include "scheduler.hpp"
#include "testing/suspension_point.hpp"

#include <cassert>
#include <utility>


namespace asyncpp {


template <class T>
class shared_task;


namespace impl_shared_task {

    template <class T>
    struct promise : result_promise<T>, resumable_promise, schedulable_promise, impl::leak_checked_promise, rc_from_this {
        struct final_awaitable {
            constexpr bool await_ready() const noexcept { return false; }
            void await_suspend(std::coroutine_handle<promise> handle) noexcept {
                auto& owner = handle.promise();
                owner.m_event.set(owner.m_result);
                owner.m_self.reset();
            }
            constexpr void await_resume() const noexcept {}
        };

        auto get_return_object() {
            return shared_task<T>(rc_ptr(this));
        }

        auto initial_suspend() noexcept {
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
        broadcast_event<T> m_event;
        rc_ptr<promise> m_self;
    };


    template <class T>
    struct awaitable : broadcast_event<T>::awaitable {
        using base = typename broadcast_event<T>::awaitable;

        rc_ptr<promise<T>> m_awaited = nullptr;

        awaitable(base base, rc_ptr<promise<T>> awaited) : broadcast_event<T>::awaitable(std::move(base)), m_awaited(awaited) {
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

} // namespace impl_shared_task


template <class T>
class shared_task {
public:
    using promise_type = impl_shared_task::promise<T>;

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