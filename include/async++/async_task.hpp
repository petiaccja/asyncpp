#pragma once

#include "awaitable.hpp"
#include "interleaving/sequence_point.hpp"
#include "promise.hpp"

#include <cassert>
#include <coroutine>
#include <future>


namespace asyncpp {


template <class T>
class async_task;


namespace impl_async_task {

    using namespace impl;

    template <class T>
    struct promise : return_promise<T>, resumable_promise, schedulable_promise {
        struct final_awaitable {
            constexpr bool await_ready() const noexcept { return false; }
            bool await_suspend(std::coroutine_handle<promise> handle) const noexcept {
                auto& owner = handle.promise();
                const auto state = INTERLEAVED(owner.m_state.exchange(READY));
                assert(state != CREATED && state != READY); // May be RUNNING || RELEASED || AWAITED
                if (state == RUNNING) {
                    return true;
                }
                else if (state == RELEASED) {
                    return false;
                }
                else {
                    auto result = owner.get_result();
                    state->on_ready(std::move(result));
                    return false;
                }
            }
            constexpr void await_resume() const noexcept {}
        };

        auto get_return_object() {
            return async_task<T>(this);
        }

        constexpr auto initial_suspend() const noexcept {
            return std::suspend_always{};
        }

        void start() noexcept {
            auto created = CREATED;
            const bool success = INTERLEAVED(m_state.compare_exchange_strong(created, RUNNING));
            if (success) {
                resume();
            }
        }

        bool await(basic_awaitable<T>* awaiter) noexcept {
            auto state = INTERLEAVED(m_state.exchange(awaiter));
            assert(state == CREATED || state == RUNNING || state == READY);
            if (state == CREATED) {
                INTERLEAVED(m_state.store(RUNNING));
                resume();
                state = INTERLEAVED(m_state.exchange(awaiter));
            }
            if (state == READY) {
                INTERLEAVED(m_state.store(READY));
            }
            return state == READY;
        }

        void release() noexcept {
            const auto state = INTERLEAVED(m_state.exchange(RELEASED));
            assert(state == CREATED || state == RUNNING || state == READY);
            if (state == CREATED || state == READY) {
                handle().destroy();
            }
        }

        auto final_suspend() noexcept {
            return final_awaitable{};
        }

        task_result<T> get_result() noexcept {
            return std::move(this->m_result);
        }

        auto handle() -> std::coroutine_handle<> final {
            return std::coroutine_handle<promise>::from_promise(*this);
        }

        void resume() final {
            [[maybe_unused]] const auto state = m_state.load();
            assert(state != READY && state != RELEASED);
            return m_scheduler ? m_scheduler->schedule(*this) : handle().resume();
        }

        bool ready() const {
            return INTERLEAVED(m_state.load()) == READY;
        }

    private:
        static inline const auto CREATED = reinterpret_cast<basic_awaitable<T>*>(std::numeric_limits<size_t>::max() - 14);
        static inline const auto RUNNING = reinterpret_cast<basic_awaitable<T>*>(std::numeric_limits<size_t>::max() - 13);
        static inline const auto READY = reinterpret_cast<basic_awaitable<T>*>(std::numeric_limits<size_t>::max() - 12);
        static inline const auto RELEASED = reinterpret_cast<basic_awaitable<T>*>(std::numeric_limits<size_t>::max() - 11);
        std::atomic<basic_awaitable<T>*> m_state = CREATED;
    };


    template <class T>
    struct awaitable : basic_awaitable<T> {
        task_result<T> m_result;
        promise<T>* m_awaited = nullptr;
        resumable_promise* m_enclosing = nullptr;

        awaitable(promise<T>* awaited) : m_awaited(awaited) {}

        bool await_ready() const noexcept {
            return false;
        }

        template <std::convertible_to<const resumable_promise&> Promise>
        bool await_suspend(std::coroutine_handle<Promise> enclosing) {
            m_enclosing = &enclosing.promise();
            const bool ready = m_awaited->await(this);
            return !ready;
        }

        T await_resume() {
            if (!m_result.has_value()) {
                m_result = m_awaited->get_result();
                m_awaited->release();
            }
            return m_result.get_or_throw();
        }

        void on_ready(task_result<T> result) noexcept final {
            m_result = std::move(result);
            m_enclosing->resume();
        }
    };


    template <class T>
    struct sync_awaitable : basic_awaitable<T> {
        promise<T>* m_awaited = nullptr;
        std::promise<task_result<T>> m_promise;
        std::future<task_result<T>> m_future = m_promise.get_future();

        sync_awaitable(promise<T>* awaited) noexcept : m_awaited(awaited) {
            const bool ready = m_awaited->await(this);
            if (ready) {
                m_promise.set_value(m_awaited->get_result());
                m_awaited->release();
            }
        }
        void on_ready(task_result<T> result) noexcept final {
            return m_promise.set_value(std::move(result));
        }
    };


} // namespace impl_async_task


template <class T>
class [[nodiscard]] async_task {
public:
    using promise_type = impl_async_task::promise<T>;

    async_task() = default;
    async_task(const async_task& rhs) = delete;
    async_task& operator=(const async_task& rhs) = delete;
    async_task(async_task&& rhs) noexcept : m_promise(std::exchange(rhs.m_promise, nullptr)) {}
    async_task& operator=(async_task&& rhs) noexcept {
        release();
        m_promise = std::exchange(rhs.m_promise, nullptr);
        return *this;
    }
    async_task(promise_type* promise) : m_promise(promise) {}
    ~async_task() { release(); }

    bool valid() const {
        return m_promise != nullptr;
    }

    void launch() {
        assert(valid());
        m_promise->start();
    }

    bool ready() const {
        assert(valid());
        return m_promise->ready();
    }

    T get() {
        assert(valid());
        impl_async_task::sync_awaitable<T> awaitable(std::exchange(m_promise, nullptr));
        return INTERLEAVED_ACQUIRE(awaitable.m_future.get()).get_or_throw();
    }

    auto operator co_await() {
        assert(valid());
        return impl_async_task::awaitable<T>(std::exchange(m_promise, nullptr));
    }

    void set_scheduler(scheduler& scheduler) {
        if (m_promise) {
            m_promise->m_scheduler = &scheduler;
        }
    }

private:
    void release() {
        if (m_promise) {
            std::exchange(m_promise, nullptr)->release();
        }
    }

private:
    promise_type* m_promise = nullptr;
};


} // namespace asyncpp