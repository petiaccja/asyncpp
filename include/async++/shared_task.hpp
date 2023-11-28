#pragma once

#include "awaitable.hpp"
#include "container/atomic_collection.hpp"
#include "interleaving/sequence_point.hpp"
#include "promise.hpp"
#include "scheduler.hpp"

#include <cassert>
#include <utility>


namespace asyncpp {


template <class T>
class shared_task;


namespace impl_shared_task {

    template <class T>
    struct chained_awaitable : basic_awaitable<T> {
        chained_awaitable* m_next;
    };

    template <class T>
    struct promise : result_promise<T>, resumable_promise, schedulable_promise {
        struct final_awaitable {
            constexpr bool await_ready() const noexcept { return false; }
            void await_suspend(std::coroutine_handle<promise> handle) noexcept {
                auto& owner = handle.promise();

                auto awaiting = INTERLEAVED(owner.m_awaiting.close());
                while (awaiting != nullptr) {
                    awaiting->on_ready();
                    awaiting = awaiting->m_next;
                }

                owner.release();
            }
            constexpr void await_resume() const noexcept {}
        };

        auto get_return_object() {
            return shared_task<T>(this);
        }

        auto initial_suspend() noexcept {
            return std::suspend_always{};
        }

        void start() noexcept {
            const bool has_started = INTERLEAVED(m_started.test_and_set());
            if (!has_started) {
                acquire();
                resume();
            }
        }

        bool await(chained_awaitable<T>* awaiter) {
            start();
            const auto previous = INTERLEAVED(m_awaiting.push(awaiter));
            return m_awaiting.closed(previous);
        }

        void acquire() {
            INTERLEAVED(m_references.fetch_add(1, std::memory_order_release));
        }

        void release() {
            const auto references = INTERLEAVED(m_references.fetch_sub(1, std::memory_order_acquire));
            if (references == 1) {
                handle().destroy();
            }
        }

        auto& get_result() noexcept {
            return this->m_result;
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

        bool ready() const {
            return INTERLEAVED(m_awaiting.closed());
        }

    private:
        std::atomic_size_t m_references = 0;
        std::atomic_flag m_started;
        atomic_collection<chained_awaitable<T>, &chained_awaitable<T>::m_next> m_awaiting;
    };


    template <class T>
    struct awaitable : chained_awaitable<T> {
        promise<T>* m_awaited = nullptr;
        resumable_promise* m_enclosing = nullptr;

        awaitable(promise<T>* awaited) : m_awaited(awaited) {}

        constexpr bool await_ready() const noexcept {
            return m_awaited->ready();
        }

        template <std::convertible_to<const resumable_promise&> Promise>
        bool await_suspend(std::coroutine_handle<Promise> enclosing) noexcept {
            m_enclosing = &enclosing.promise();
            const bool ready = m_awaited->await(this);
            return !ready;
        }

        auto await_resume() -> typename task_result<T>::reference {
            return m_awaited->get_result().get_or_throw();
        }

        void on_ready() noexcept final {
            m_enclosing->resume();
        }
    };

} // namespace impl_shared_task


template <class T>
class shared_task {
public:
    using promise_type = impl_shared_task::promise<T>;

    shared_task() = default;

    shared_task(const shared_task& rhs) noexcept : shared_task(rhs.m_promise) {}

    shared_task& operator=(const shared_task& rhs) noexcept {
        if (m_promise) {
            m_promise->release();
        }
        m_promise = rhs.m_promise;
        m_promise->acquire();
        return *this;
    }

    shared_task(shared_task&& rhs) noexcept : m_promise(std::exchange(rhs.m_promise, nullptr)) {}

    shared_task& operator=(shared_task&& rhs) noexcept {
        if (m_promise) {
            m_promise->release();
        }
        m_promise = std::exchange(rhs.m_promise, nullptr);
        return *this;
    }

    shared_task(promise_type* promise) : m_promise(promise) {
        m_promise->acquire();
    }

    ~shared_task() {
        if (m_promise) {
            m_promise->release();
        }
    }

    bool valid() const {
        return m_promise != nullptr;
    }

    void launch() {
        assert(valid());
        m_promise->start();
    }

    bool ready() {
        assert(valid());
        return m_promise->ready();
    }

    auto operator co_await() const {
        assert(valid());
        return impl_shared_task::awaitable<T>(m_promise);
    }

    void bind(scheduler& scheduler) {
        if (m_promise) {
            m_promise->m_scheduler = &scheduler;
        }
    }

private:
    promise_type* m_promise;
};


} // namespace asyncpp