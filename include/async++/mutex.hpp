#pragma once

#include "container/atomic_queue.hpp"
#include "promise.hpp"
#include "sync/spinlock.hpp"

#include <cassert>
#include <concepts>
#include <mutex>
#include <utility>


namespace asyncpp {

template <class Mutex, void (Mutex::*unlock)()>
class [[nodiscard]] lock {
public:
    lock(Mutex* mtx) : m_mtx(mtx) {}
    lock(lock&& rhs) : m_mtx(std::exchange(rhs.m_mtx, nullptr)) {}
    lock& operator=(lock&& rhs) {
        if (m_mtx) {
            m_mtx->unlock();
        }
        m_mtx = std::exchange(rhs.m_mtx, nullptr);
        return *this;
    }
    ~lock() {
        if (m_mtx) {
            (m_mtx->*unlock)();
        }
    }
    Mutex& parent() const noexcept {
        return *m_mtx;
    }

private:
    Mutex* m_mtx = nullptr;
};


class mutex {
    void unlock();

public:
    using lock = lock<mutex, &mutex::unlock>;
    friend lock;

private:
    struct awaitable {
        awaitable* m_next = nullptr;
        awaitable* m_prev = nullptr;

        awaitable(mutex* mtx) : m_mtx(mtx) {}
        bool await_ready() noexcept;
        template <std::convertible_to<const impl::resumable_promise&> Promise>
        bool await_suspend(std::coroutine_handle<Promise> enclosing) noexcept;
        lock await_resume() noexcept;
        void on_ready(lock lk) noexcept;

    private:
        mutex* m_mtx;
        impl::resumable_promise* m_enclosing = nullptr;
        std::optional<lock> m_lk;
    };

public:
    [[nodiscard]] std::optional<lock> try_lock() noexcept;
    awaitable unique() noexcept;
    awaitable operator co_await() noexcept;

private:
    std::optional<lock> wait(awaitable* waiting);

private:
    atomic_queue<awaitable, &awaitable::m_next, &awaitable::m_prev> m_queue;
    bool m_locked = false;
    spinlock m_spinlock;
};


template <std::convertible_to<const impl::resumable_promise&> Promise>
bool mutex::awaitable::await_suspend(std::coroutine_handle<Promise> enclosing) noexcept {
    m_enclosing = &enclosing.promise();
    m_lk = m_mtx->wait(this);
    return !m_lk.has_value();
}


template <class Mutex>
class unique_lock {
    using mutex_awaitable = std::invoke_result_t<decltype(&Mutex::operator co_await), Mutex*>;
    struct awaitable {
        unique_lock* m_lock;
        mutex_awaitable m_awaitable;

        auto await_ready() noexcept {
            return m_awaitable.await_ready();
        }

        template <class Promise>
        auto await_suspend(std::coroutine_handle<Promise> enclosing) noexcept {
            return m_awaitable.await_suspend(enclosing);
        }

        void await_resume() noexcept {
            m_lock->m_lk = m_awaitable.await_resume();
        }
    };

public:
    unique_lock(Mutex& mtx) noexcept : m_mtx(mtx) {}

    template <void (Mutex::*unlock)()>
    unique_lock(lock<Mutex, unlock> lk) noexcept : m_mtx(lk.parent()), m_lk(std::move(lk)) {}

    bool try_lock() noexcept {
        assert(!owns_lock());
        m_lk = m_mtx.try_lock();
        return m_lk.has_value();
    }

    auto operator co_await() noexcept {
        assert(!owns_lock());
        return awaitable(this, m_mtx.unique());
    }

    void unlock() noexcept {
        assert(owns_lock());
        m_lk = std::nullopt;
    }

    Mutex& mutex() const noexcept {
        return m_mtx;
    }

    bool owns_lock() const noexcept {
        return m_lk.has_value();
    }

    operator bool() const noexcept {
        return owns_lock();
    }

private:
    Mutex& m_mtx;
    std::optional<typename Mutex::lock> m_lk;
};


template <class Mutex_, void (Mutex_::*unlock)()>
unique_lock(lock<Mutex_, unlock> lk) -> unique_lock<Mutex_>;

} // namespace asyncpp