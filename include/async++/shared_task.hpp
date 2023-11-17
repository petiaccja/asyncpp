#pragma once

#include "awaitable.hpp"
#include "interleaving/sequence_point.hpp"
#include "promise.hpp"
#include "scheduler.hpp"
#include "container/atomic_collection.hpp"

#include <cassert>
#include <future>
#include <utility>


namespace asyncpp {


template <class T>
class shared_task;


namespace impl_shared_task {

    using namespace impl;

    template <class T>
    struct chained_awaitable : basic_awaitable<T> {
        chained_awaitable* m_next;
    };

    template <class T>
    struct promise : resumable_promise, schedulable_promise, return_promise<T> {
        struct final_awaitable {
            constexpr bool await_ready() const noexcept { return false; }
            void await_suspend(std::coroutine_handle<promise> handle) noexcept {
                auto& owner = handle.promise();

                auto awaiting = INTERLEAVED(owner.m_awaiting.close());
                const auto state = INTERLEAVED(owner.m_state.exchange(owner.READY));
                while (awaiting != nullptr) {
                    if (!(state == owner.EXCLUDE_FIRST && awaiting->m_next == nullptr)) {
                        awaiting->on_ready();
                    }
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
            acquire();
            return std::suspend_always{};
        }

        void start() noexcept {
            auto created = CREATED;
            const bool success = INTERLEAVED(m_state.compare_exchange_strong(created, RUNNING));
            if (success) {
                resume();
            }
        }

        bool await(chained_awaitable<T>* awaiter) {
            const auto previous = INTERLEAVED(m_awaiting.push(awaiter));
            if (previous == nullptr) {
                auto created = CREATED;
                const bool was_created = INTERLEAVED(m_state.compare_exchange_strong(created, EXCLUDE_FIRST));
                if (was_created == CREATED) {
                    resume();
                    auto exclude_first = EXCLUDE_FIRST;
                    INTERLEAVED(m_state.compare_exchange_strong(exclude_first, RUNNING));
                    if (exclude_first == READY) {
                        return true;
                    }
                }
            }
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
            return m_awaiting.closed();
        }

    private:
        static constexpr int CREATED = 1;
        static constexpr int RUNNING = 2;
        static constexpr int EXCLUDE_FIRST = 3;
        static constexpr int READY = 4;
        std::atomic_size_t m_references = 0;
        std::atomic_int m_state = CREATED;
        atomic_collection<chained_awaitable<T>, &chained_awaitable<T>::m_next> m_awaiting;
    };


    template <class T>
    struct awaitable : chained_awaitable<T> {
        task_result<T> m_result;
        promise<T>* m_awaited = nullptr;
        resumable_promise* m_enclosing = nullptr;

        awaitable(promise<T>* awaited) : m_awaited(awaited) {
            m_awaited->acquire();
        }
        ~awaitable() override {
            m_awaited->release();
        }

        constexpr bool await_ready() const noexcept {
            return false;
        }

        template <std::convertible_to<const resumable_promise&> Promise>
        bool await_suspend(std::coroutine_handle<Promise> enclosing) {
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


    template <class T>
    struct sync_awaitable : chained_awaitable<T> {
        promise<T>* m_awaited = nullptr;
        std::promise<task_result<T>&> m_promise;
        std::future<task_result<T>&> m_future = m_promise.get_future();

        sync_awaitable(promise<T>* awaited) noexcept : m_awaited(awaited) {
            m_awaited->acquire();
            const bool ready = m_awaited->await(this);
            if (ready) {
                m_promise.set_value(m_awaited->get_result());
            }
        }
        ~sync_awaitable() override {
            m_awaited->release();
        }
        void on_ready() noexcept final {
            return m_promise.set_value(m_awaited->get_result());
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

    auto get() const -> typename impl::task_result<T>::reference {
        assert(valid());
        impl_shared_task::sync_awaitable<T> awaitable(m_promise);
        return INTERLEAVED_ACQUIRE(awaitable.m_future.get()).get_or_throw();
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