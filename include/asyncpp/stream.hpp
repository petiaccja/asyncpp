#pragma once

#include "event.hpp"
#include "memory/rc_ptr.hpp"
#include "promise.hpp"
#include "scheduler.hpp"

#include <coroutine>
#include <exception>
#include <optional>
#include <utility>


namespace asyncpp {


template <class T, class Alloc>
class stream;


namespace impl_stream {

    template <class T>
    using wrapper_type = typename task_result<T>::wrapper_type;

    template <class T>
    using reference_type = typename task_result<T>::reference;


    template <class T>
    struct [[nodiscard]] item {
        item(std::optional<wrapper_type<T>> result) : m_result(std::move(result)) {}

        explicit operator bool() const noexcept {
            return !!m_result;
        }

        T& operator*() noexcept {
            assert(m_result);
            return m_result.value();
        }

        std::remove_reference_t<T>* operator->() noexcept {
            assert(m_result);
            return std::addressof(static_cast<T&>(m_result.value()));
        }

        const T& operator*() const noexcept {
            assert(m_result);
            return m_result.value();
        }

        const std::remove_reference_t<T>* operator->() const noexcept {
            assert(m_result);
            return std::addressof(static_cast<const T&>(m_result.value()));
        }

    private:
        std::optional<wrapper_type<T>> m_result;
    };


    template <class T, class Alloc>
    struct promise : resumable_promise, schedulable_promise, rc_from_this, allocator_aware_promise<Alloc> {
        struct yield_awaitable {
            constexpr bool await_ready() const noexcept { return false; }

            void await_suspend(std::coroutine_handle<promise> handle) const noexcept {
                auto& owner = handle.promise();
                assert(owner.m_event);
                assert(owner.m_result.has_value());
                owner.m_event->set(std::move(owner.m_result));
            }

            constexpr void await_resume() const noexcept {}
        };

        auto get_return_object() noexcept {
            return stream<T, Alloc>(rc_ptr(this));
        }

        constexpr std::suspend_always initial_suspend() const noexcept {
            return {};
        }

        yield_awaitable final_suspend() const noexcept {
            return {};
        }

        yield_awaitable yield_value(T value) noexcept {
            m_result = std::optional(wrapper_type<T>(std::forward<T>(value)));
            return {};
        }

        void unhandled_exception() noexcept {
            m_result = std::current_exception();
        }

        void return_void() noexcept {
            m_result = std::nullopt;
        }

        void resume() final {
            return m_scheduler ? m_scheduler->schedule(*this) : resume_now();
        }

        void resume_now() final {
            const auto handle = std::coroutine_handle<promise>::from_promise(*this);
            handle.resume();
        }

        void destroy() noexcept {
            const auto handle = std::coroutine_handle<promise>::from_promise(*this);
            handle.destroy();
        }

        auto await() noexcept;

        bool has_event() const noexcept {
            return !!m_event;
        }

    private:
        std::optional<event<std::optional<wrapper_type<T>>>> m_event;
        task_result<std::optional<wrapper_type<T>>> m_result;
    };


    template <class T, class Alloc>
    struct awaitable {
        using base = typename event<std::optional<wrapper_type<T>>>::awaitable;

        base m_base;
        rc_ptr<promise<T, Alloc>> m_awaited = nullptr;

        awaitable(base base, rc_ptr<promise<T, Alloc>> awaited) : m_base(base), m_awaited(awaited) {}

        bool await_ready() noexcept {
            assert(m_awaited->has_event());
            return m_base.await_ready();
        }

        template <std::convertible_to<const resumable_promise&> Promise>
        bool await_suspend(std::coroutine_handle<Promise> enclosing) {
            assert(m_awaited->has_event());
            return m_base.await_suspend(enclosing);
        }

        item<T> await_resume() {
            assert(m_awaited->has_event());
            return { m_base.await_resume() };
        }
    };


    template <class T, class Alloc>
    auto promise<T, Alloc>::await() noexcept {
        m_event.emplace();
        auto aw = awaitable<T, Alloc>(m_event->operator co_await(), rc_ptr(this));
        resume();
        return aw;
    }

} // namespace impl_stream


template <class T, class Alloc = void>
class [[nodiscard]] stream {
public:
    using promise_type = impl_stream::promise<T, Alloc>;

    stream() = default;
    stream(const stream&) = delete;
    stream& operator=(const stream&) = delete;
    stream(stream&& rhs) noexcept = default;
    stream& operator=(stream&& rhs) noexcept = default;
    stream(rc_ptr<promise_type> promise) : m_promise(std::move(promise)) {}

    auto operator co_await() const {
        return m_promise->await();
    }

    bool valid() const {
        return !!m_promise;
    }

private:
    rc_ptr<promise_type> m_promise;
};


} // namespace asyncpp