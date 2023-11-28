#pragma once

#include "awaitable.hpp"
#include "interleaving/sequence_point.hpp"
#include "promise.hpp"
#include "scheduler.hpp"

#include <atomic>
#include <cassert>
#include <coroutine>
#include <limits>
#include <utility>


namespace asyncpp {


template <class T>
class task;


namespace impl_task {

    using namespace impl;

    template <class T>
    struct promise : return_promise<T>, resumable_promise, schedulable_promise {
        struct final_awaitable {
            constexpr bool await_ready() const noexcept { return false; }
            void await_suspend(std::coroutine_handle<promise> handle) const noexcept {
                auto& owner = handle.promise();
                const auto state = INTERLEAVED(owner.m_state.exchange(READY));
                assert(state != CREATED && state != READY); // May be RUNNING || AWAITED
                if (state != RUNNING) {
                    state->on_ready();
                }
                owner.release();
            }
            constexpr void await_resume() const noexcept {}
        };

        auto get_return_object() {
            return task<T>(this);
        }

        constexpr auto initial_suspend() noexcept {
            m_released.test_and_set();
            return std::suspend_always{};
        }

        void start() noexcept {
            auto created = CREATED;
            const bool success = INTERLEAVED(m_state.compare_exchange_strong(created, RUNNING));
            if (success) {
                m_released.clear();
                resume();
            }
        }

        bool await(basic_awaitable<T>* awaiter) noexcept {
            start();
            auto state = INTERLEAVED(m_state.exchange(awaiter));
            assert(state == CREATED || state == RUNNING || state == READY);
            if (state == READY) {
                INTERLEAVED(m_state.store(READY));
            }
            return state == READY;
        }

        void release() noexcept {
            const auto released = m_released.test_and_set();
            if (released) {
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
            assert(state != READY);
            return m_scheduler ? m_scheduler->schedule(*this) : handle().resume();
        }

        bool ready() const {
            return INTERLEAVED(m_state.load()) == READY;
        }

    private:
        static inline const auto CREATED = reinterpret_cast<basic_awaitable<T>*>(std::numeric_limits<size_t>::max() - 14);
        static inline const auto RUNNING = reinterpret_cast<basic_awaitable<T>*>(std::numeric_limits<size_t>::max() - 13);
        static inline const auto READY = reinterpret_cast<basic_awaitable<T>*>(std::numeric_limits<size_t>::max() - 12);
        std::atomic<basic_awaitable<T>*> m_state = CREATED;
        std::atomic_flag m_released;
    };


    template <class T>
    struct awaitable : basic_awaitable<T> {
        promise<T>* m_awaited = nullptr;
        resumable_promise* m_enclosing = nullptr;

        awaitable(promise<T>* awaited) : m_awaited(awaited) {}

        constexpr bool await_ready() const noexcept {
            return m_awaited->ready();
        }

        template <std::convertible_to<const resumable_promise&> Promise>
        bool await_suspend(std::coroutine_handle<Promise> enclosing) {
            m_enclosing = &enclosing.promise();
            const bool ready = m_awaited->await(this);
            return !ready;
        }

        T await_resume() {
            auto result = m_awaited->get_result();
            m_awaited->release();
            if constexpr (!std::is_void_v<T>) {
                return std::forward<T>(result.get_or_throw());
            }
            else {
                return result.get_or_throw();
            }
        }

        void on_ready() noexcept final {
            m_enclosing->resume();
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
        release();
        m_promise = std::exchange(rhs.m_promise, nullptr);
        return *this;
    }
    task(promise_type* promise) : m_promise(promise) {}
    ~task() { release(); }

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

    auto operator co_await() {
        assert(valid());
        return impl_task::awaitable<T>(std::exchange(m_promise, nullptr));
    }

    void bind(scheduler& scheduler) {
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