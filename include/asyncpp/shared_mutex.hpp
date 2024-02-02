#pragma once

#include "container/atomic_queue.hpp"
#include "lock.hpp"
#include "promise.hpp"
#include "threading/spinlock.hpp"

#include <concepts>
#include <optional>


namespace asyncpp {

class shared_mutex {
    struct basic_awaitable {
        basic_awaitable* m_next = nullptr;
        basic_awaitable* m_prev = nullptr;

        basic_awaitable(shared_mutex* mtx) : m_mtx(mtx) {}
        virtual ~basic_awaitable() = default;
        virtual void on_ready() noexcept = 0;
        virtual bool is_shared() const noexcept = 0;

    protected:
        shared_mutex* m_mtx;
    };

    struct awaitable : basic_awaitable {
        using basic_awaitable::basic_awaitable;

        bool await_ready() noexcept;
        template <std::convertible_to<const resumable_promise&> Promise>
        bool await_suspend(std::coroutine_handle<Promise> enclosing) noexcept;
        locked_mutex<shared_mutex> await_resume() noexcept;
        void on_ready() noexcept final;
        bool is_shared() const noexcept final;

    private:
        resumable_promise* m_enclosing = nullptr;
    };

    struct shared_awaitable : basic_awaitable {
        using basic_awaitable::basic_awaitable;

        bool await_ready() noexcept;
        template <std::convertible_to<const resumable_promise&> Promise>
        bool await_suspend(std::coroutine_handle<Promise> enclosing) noexcept;
        locked_mutex_shared<shared_mutex> await_resume() noexcept;
        void on_ready() noexcept final;
        bool is_shared() const noexcept final;

    private:
        resumable_promise* m_enclosing = nullptr;
    };

    bool lock_enqueue(awaitable* waiting);
    bool lock_enqueue_shared(shared_awaitable* waiting);

public:
    shared_mutex() = default;
    shared_mutex(const shared_mutex&) = delete;
    shared_mutex(shared_mutex&&) = delete;
    shared_mutex& operator=(const shared_mutex&) = delete;
    shared_mutex& operator=(shared_mutex&&) = delete;
    ~shared_mutex();

    bool try_lock() noexcept;
    bool try_lock_shared() noexcept;
    awaitable unique() noexcept;
    shared_awaitable shared() noexcept;
    void unlock();
    void unlock_shared();


private:
    using queue_t = atomic_queue<basic_awaitable, &basic_awaitable::m_next, &basic_awaitable::m_prev>;
    queue_t m_queue;
    intptr_t m_locked = 0;
    intptr_t m_unique_waiting = false;
    spinlock m_spinlock;
};


template <std::convertible_to<const resumable_promise&> Promise>
bool shared_mutex::awaitable::await_suspend(std::coroutine_handle<Promise> enclosing) noexcept {
    m_enclosing = &enclosing.promise();
    const bool ready = m_mtx->lock_enqueue(this);
    return !ready;
}


template <std::convertible_to<const resumable_promise&> Promise>
bool shared_mutex::shared_awaitable::await_suspend(std::coroutine_handle<Promise> enclosing) noexcept {
    m_enclosing = &enclosing.promise();
    const bool ready = m_mtx->lock_enqueue_shared(this);
    return !ready;
}


} // namespace asyncpp