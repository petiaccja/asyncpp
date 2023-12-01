#pragma once

#include "awaitable.hpp"
#include "interleaving/sequence_point.hpp"
#include "promise.hpp"
#include "scheduler.hpp"

#include <cassert>
#include <coroutine>
#include <exception>
#include <future>
#include <optional>
#include <utility>


namespace asyncpp {


template <class T>
class stream;


namespace impl_stream {

    template <class T>
    struct promise : resumable_promise, schedulable_promise, impl::leak_checked_promise {
        struct yield_awaitable {
            constexpr bool await_ready() const noexcept {
                return false;
            }

            void await_suspend(std::coroutine_handle<promise> handle) const noexcept {
                auto& owner = handle.promise();
                const auto state = INTERLEAVED(owner.m_state.exchange(READY));
                assert(state != STOPPED && state != READY);
                if (state != RUNNING) {
                    state->on_ready();
                }
            }

            constexpr void await_resume() const noexcept {}
        };

        auto get_return_object() noexcept {
            return stream<T>(this);
        }

        constexpr auto initial_suspend() const noexcept {
            return std::suspend_always{};
        }

        bool await(basic_awaitable<T>* awaiting) {
            INTERLEAVED(m_state.store(RUNNING));
            resume();
            const auto state = INTERLEAVED(m_state.exchange(awaiting));
            assert(state == RUNNING || state == READY);
            return state == READY;
        }

        auto get_result() {
            return std::move(m_result);
        }

        auto final_suspend() const noexcept {
            return yield_awaitable{};
        }

        void release() noexcept {
            handle().destroy();
        }

        auto handle() -> std::coroutine_handle<> override {
            return std::coroutine_handle<promise>::from_promise(*this);
        }

        void resume() override {
            return m_scheduler ? m_scheduler->schedule(*this) : handle().resume();
        }

        void unhandled_exception() noexcept {
            m_result = std::current_exception();
        }

        auto yield_value(T value) noexcept {
            m_result = std::forward<T>(value);
            return yield_awaitable{};
        }

        void return_void() noexcept {
            m_result.clear();
        }

    private:
        static inline const auto STOPPED = reinterpret_cast<basic_awaitable<T>*>(std::numeric_limits<size_t>::max() - 14);
        static inline const auto RUNNING = reinterpret_cast<basic_awaitable<T>*>(std::numeric_limits<size_t>::max() - 13);
        static inline const auto READY = reinterpret_cast<basic_awaitable<T>*>(std::numeric_limits<size_t>::max() - 12);
        std::atomic<basic_awaitable<T>*> m_state = STOPPED;
        task_result<T> m_result;
    };


    template <class T>
    struct awaitable : basic_awaitable<T> {
        task_result<T> m_result;
        promise<T>* m_awaited = nullptr;
        resumable_promise* m_enclosing = nullptr;

        awaitable(promise<T>* awaited) : m_awaited(awaited) {}

        bool await_ready() noexcept {
            return false;
        }

        template <std::convertible_to<const resumable_promise&> Promise>
        bool await_suspend(std::coroutine_handle<Promise> enclosing) {
            m_enclosing = &enclosing.promise();
            const bool ready = m_awaited->await(this);
            return !ready;
        }

        auto await_resume() -> std::optional<typename task_result<T>::wrapper_type> {
            // Result was ready in await suspend or result was nullopt.
            if (!m_result.has_value()) {
                m_result = m_awaited->get_result();
            }
            // Ensure result was truly nullopt.
            if (!m_result.has_value()) {
                return std::nullopt;
            }
            return m_result.get_or_throw();
        }

        void on_ready() noexcept final {
            m_result = m_awaited->get_result();
            m_enclosing->resume();
        }
    };

} // namespace impl_stream


template <class T>
class [[nodiscard]] stream {
public:
    using promise_type = impl_stream::promise<T>;

    stream() = default;
    stream(const stream&) = delete;
    stream& operator=(const stream&) = delete;
    stream(stream&& rhs) noexcept : m_promise(std::exchange(rhs.m_promise, nullptr)) {}
    stream& operator=(stream&& rhs) noexcept {
        release();
        m_promise = std::exchange(rhs.m_promise, nullptr);
        return *this;
    }
    stream(promise_type* promise) : m_promise(promise) {}
    ~stream() {
        release();
    }

    auto operator co_await() const {
        return impl_stream::awaitable<T>(m_promise);
    }

    operator bool() const {
        return good();
    }

    bool good() const {
        return m_promise != nullptr;
    }

private:
    void release() {
        if (m_promise) {
            m_promise->release();
        }
    }

private:
    promise_type* m_promise = nullptr;
};


} // namespace asyncpp