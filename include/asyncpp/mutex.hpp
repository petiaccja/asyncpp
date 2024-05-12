#pragma once

#include "container/atomic_deque.hpp"
#include "lock.hpp"
#include "promise.hpp"
#include "threading/spinlock.hpp"

#include <concepts>


namespace asyncpp {

class mutex {
    struct awaitable {
        mutex* m_owner = nullptr;
        resumable_promise* m_enclosing = nullptr;
        awaitable* m_next = nullptr;
        awaitable* m_prev = nullptr;

        bool await_ready() const noexcept;

        template <std::convertible_to<const resumable_promise&> Promise>
        bool await_suspend(std::coroutine_handle<Promise> enclosing) noexcept;

        exclusively_locked_mutex<mutex> await_resume() noexcept;
    };

    bool add_awaiting(awaitable* waiting);

public:
    mutex() = default;
    mutex(const mutex&) = delete;
    mutex(mutex&&) = delete;
    mutex& operator=(const mutex&) = delete;
    mutex& operator=(mutex&&) = delete;
    ~mutex();

    bool try_lock() noexcept;
    awaitable exclusive() noexcept;
    awaitable operator co_await() noexcept;
    void unlock();

    void _debug_clear() noexcept;
    bool _debug_is_locked() noexcept;

private:
    // Front: the next awaiting coroutine to acquire, back: last coroutine to acquire.
    deque<awaitable, &awaitable::m_prev, &awaitable::m_next> m_queue;
    // Used to protect the queue data structure.
    spinlock m_spinlock;
    // At the front, signifies mutex is locked.
    awaitable m_sentinel;
};


template <std::convertible_to<const resumable_promise&> Promise>
bool mutex::awaitable::await_suspend(std::coroutine_handle<Promise> enclosing) noexcept {
    assert(m_owner);
    m_enclosing = &enclosing.promise();
    const bool ready = m_owner->add_awaiting(this);
    return !ready;
}

} // namespace asyncpp