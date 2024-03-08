#pragma once

#include "container/atomic_deque.hpp"
#include "promise.hpp"
#include "threading/spinlock.hpp"

#include <cassert>
#include <cstdint>


namespace asyncpp {

class counting_semaphore {
    struct awaitable {
        counting_semaphore* m_owner = nullptr;
        resumable_promise* m_enclosing = nullptr;
        awaitable* m_prev = nullptr;
        awaitable* m_next = nullptr;

        bool await_ready() const noexcept;

        template <std::convertible_to<const resumable_promise&> Promise>
        bool await_suspend(std::coroutine_handle<Promise> promise) noexcept {
            assert(m_owner);
            m_enclosing = &promise.promise();
            return !m_owner->acquire(this);
        }

        constexpr void await_resume() const noexcept {}
    };

public:
    counting_semaphore(ptrdiff_t current_counter = 0, ptrdiff_t max_counter = std::numeric_limits<ptrdiff_t>::max()) noexcept;

    bool try_acquire() noexcept;
    awaitable operator co_await() noexcept;
    void release() noexcept;
    ptrdiff_t max() const noexcept;

    ptrdiff_t _debug_get_counter() const noexcept;
    deque<awaitable, &awaitable::m_prev, &awaitable::m_next>& _debug_get_awaiters();
    void _debug_clear();

private:
    bool acquire(awaitable* waiting) noexcept;

private:
    spinlock m_spinlock;
    deque<awaitable, &awaitable::m_prev, &awaitable::m_next> m_awaiters;
    ptrdiff_t m_counter = 0;
    const ptrdiff_t m_max;
};

} // namespace asyncpp