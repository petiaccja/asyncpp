#pragma once

#include "container/atomic_queue.hpp"
#include "lock.hpp"
#include "promise.hpp"
#include "threading/spinlock.hpp"

#include <concepts>
#include <optional>


namespace asyncpp {

class mutex {
    struct awaitable {
        awaitable* m_next = nullptr;
        awaitable* m_prev = nullptr;

        awaitable(mutex* mtx) : m_mtx(mtx) {}
        bool await_ready() noexcept;
        template <std::convertible_to<const impl::resumable_promise&> Promise>
        bool await_suspend(std::coroutine_handle<Promise> enclosing) noexcept;
        mutex_lock<mutex> await_resume() noexcept;
        void on_ready() noexcept;

    private:
        mutex* m_mtx;
        impl::resumable_promise* m_enclosing = nullptr;
    };

    bool lock_enqueue(awaitable* waiting);

public:
    bool try_lock() noexcept;
    awaitable unique() noexcept;
    awaitable operator co_await() noexcept;
    void unlock();

private:
    atomic_queue<awaitable, &awaitable::m_next, &awaitable::m_prev> m_queue;
    bool m_locked = false;
    spinlock m_spinlock;
};


template <std::convertible_to<const impl::resumable_promise&> Promise>
bool mutex::awaitable::await_suspend(std::coroutine_handle<Promise> enclosing) noexcept {
    m_enclosing = &enclosing.promise();
    const bool ready = m_mtx->lock_enqueue(this);
    return !ready;
}

} // namespace asyncpp