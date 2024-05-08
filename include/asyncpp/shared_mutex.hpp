#pragma once

#include "container/atomic_deque.hpp"
#include "lock.hpp"
#include "promise.hpp"
#include "threading/spinlock.hpp"

#include <concepts>
#include <optional>


namespace asyncpp {

class shared_mutex {
    enum class awaitable_type {
        exclusive,
        shared,
        unknown,
    };

    struct basic_awaitable {
        shared_mutex* m_owner = nullptr;
        awaitable_type m_type = awaitable_type::unknown;
        basic_awaitable* m_next = nullptr;
        basic_awaitable* m_prev = nullptr;
        resumable_promise* m_enclosing = nullptr;

        template <std::convertible_to<const resumable_promise&> Promise>
        bool await_suspend(std::coroutine_handle<Promise> enclosing) noexcept;
    };

    struct exclusive_awaitable : basic_awaitable {
        explicit exclusive_awaitable(shared_mutex* owner = nullptr)
            : basic_awaitable(owner, awaitable_type::exclusive) {}

        bool await_ready() const noexcept;
        exclusively_locked_mutex<shared_mutex> await_resume() const noexcept;
    };

    struct shared_awaitable : basic_awaitable {
        explicit shared_awaitable(shared_mutex* owner = nullptr)
            : basic_awaitable(owner, awaitable_type::shared) {}

        bool await_ready() const noexcept;
        shared_locked_mutex<shared_mutex> await_resume() const noexcept;
    };

    bool add_awaiting(basic_awaitable* waiting);
    void continue_waiting(std::unique_lock<spinlock>& lk);

public:
    shared_mutex() = default;
    shared_mutex(const shared_mutex&) = delete;
    shared_mutex(shared_mutex&&) = delete;
    shared_mutex& operator=(const shared_mutex&) = delete;
    shared_mutex& operator=(shared_mutex&&) = delete;
    ~shared_mutex();

    bool try_lock() noexcept;
    bool try_lock_shared() noexcept;
    exclusive_awaitable exclusive() noexcept;
    shared_awaitable shared() noexcept;
    void unlock();
    void unlock_shared();

    void _debug_clear() noexcept;
    bool _debug_is_exclusive_locked() const noexcept;
    size_t _debug_is_shared_locked() const noexcept;

private:
    deque<basic_awaitable, &basic_awaitable::m_prev, &basic_awaitable::m_next> m_queue;
    spinlock m_spinlock;
    exclusive_awaitable m_exclusive_sentinel;
    shared_awaitable m_shared_sentinel;
    size_t m_shared_count = 0;
};


template <std::convertible_to<const resumable_promise&> Promise>
bool shared_mutex::basic_awaitable::await_suspend(std::coroutine_handle<Promise> enclosing) noexcept {
    m_enclosing = &enclosing.promise();
    assert(m_owner);
    const bool ready = m_owner->add_awaiting(this);
    return !ready;
}


} // namespace asyncpp