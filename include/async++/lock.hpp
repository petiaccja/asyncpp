#pragma once

#include <cassert>
#include <coroutine>
#include <optional>
#include <utility>


namespace asyncpp {

template <class Mutex>
class mutex_lock {
    friend Mutex;

public:
    mutex_lock(mutex_lock&&) = default;
    mutex_lock& operator=(mutex_lock&&) = default;
    mutex_lock(const mutex_lock&) = delete;
    mutex_lock& operator=(const mutex_lock&) = delete;
    Mutex& mutex() const noexcept {
        return *m_mtx;
    }

private:
    mutex_lock(Mutex* mtx) : m_mtx(mtx) {}
    Mutex* m_mtx = nullptr;
};


template <class Mutex>
class mutex_shared_lock {
    friend Mutex;

public:
    mutex_shared_lock(mutex_shared_lock&&) = default;
    mutex_shared_lock& operator=(mutex_shared_lock&&) = default;
    mutex_shared_lock(const mutex_shared_lock&) = delete;
    mutex_shared_lock& operator=(const mutex_shared_lock&) = delete;
    Mutex& mutex() const noexcept {
        return *m_mtx;
    }

private:
    mutex_shared_lock(Mutex* mtx) : m_mtx(mtx) {}
    Mutex* m_mtx = nullptr;
};


template <class Mutex>
class unique_lock {
    using mutex_awaitable = std::invoke_result_t<decltype(&Mutex::unique), Mutex*>;
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
            m_awaitable.await_resume();
            m_lock->m_owned = true;
        }
    };

public:
    unique_lock(Mutex& mtx) noexcept : m_mtx(&mtx) {}
    unique_lock(mutex_lock<Mutex>&& lk) noexcept : m_mtx(&lk.mutex()), m_owned(true) {}
    unique_lock(unique_lock&& rhs) noexcept : m_mtx(rhs.m_mtx), m_owned(rhs.m_owned) {
        rhs.m_mtx = nullptr;
        rhs.m_owned = false;
    }
    unique_lock& operator=(unique_lock&& rhs) noexcept {
        if (owns_lock()) {
            m_mtx->unlock();
        }
        m_mtx = std::exchange(rhs.m_mtx, nullptr);
        m_owned = std::exchange(rhs.m_owned, false);
        return *this;
    }
    unique_lock(const unique_lock& rhs) = delete;
    unique_lock& operator=(const unique_lock& rhs) = delete;
    ~unique_lock() {
        if (owns_lock()) {
            m_mtx->unlock();
        }
    }

    bool try_lock() noexcept {
        assert(!owns_lock());
        m_owned = m_mtx->try_lock();
        return m_owned;
    }

    auto operator co_await() noexcept {
        assert(!owns_lock());
        return awaitable(this, m_mtx->unique());
    }

    void unlock() noexcept {
        assert(owns_lock());
        m_mtx->unlock();
        m_owned = false;
    }

    Mutex& mutex() const noexcept {
        return *m_mtx;
    }

    bool owns_lock() const noexcept {
        return m_owned;
    }

    operator bool() const noexcept {
        return owns_lock();
    }

private:
    Mutex* m_mtx = nullptr;
    bool m_owned = false;
};


template <class Mutex>
unique_lock(mutex_lock<Mutex>&& lk) -> unique_lock<Mutex>;


template <class Mutex>
class shared_lock {
    using mutex_awaitable = std::invoke_result_t<decltype(&Mutex::shared), Mutex*>;
    struct awaitable {
        shared_lock* m_lock;
        mutex_awaitable m_awaitable;

        auto await_ready() noexcept {
            return m_awaitable.await_ready();
        }

        template <class Promise>
        auto await_suspend(std::coroutine_handle<Promise> enclosing) noexcept {
            return m_awaitable.await_suspend(enclosing);
        }

        void await_resume() noexcept {
            m_awaitable.await_resume();
            m_lock->m_owned = true;
        }
    };

public:
    shared_lock(Mutex& mtx) noexcept : m_mtx(&mtx) {}
    shared_lock(mutex_shared_lock<Mutex> lk) noexcept : m_mtx(&lk.mutex()), m_owned(true) {}
    shared_lock(shared_lock&& rhs) noexcept : m_mtx(rhs.m_mtx), m_owned(rhs.m_owned) {
        rhs.m_mtx = nullptr;
        rhs.m_owned = false;
    }
    shared_lock& operator=(shared_lock&& rhs) noexcept {
        if (owns_lock()) {
            m_mtx->unlock_shared();
        }
        m_mtx = std::exchange(rhs.m_mtx, nullptr);
        m_owned = std::exchange(rhs.m_owned, false);
        return *this;
    }
    shared_lock(const shared_lock& rhs) = delete;
    shared_lock& operator=(const shared_lock& rhs) = delete;
    ~shared_lock() {
        if (owns_lock()) {
            m_mtx->unlock_shared();
        }
    }

    bool try_lock() noexcept {
        assert(!owns_lock());
        m_owned = m_mtx->try_lock_shared();
        return m_owned;
    }

    auto operator co_await() noexcept {
        assert(!owns_lock());
        return awaitable(this, m_mtx->shared());
    }

    void unlock() noexcept {
        assert(owns_lock());
        m_mtx->unlock_shared();
        m_owned = false;
    }

    Mutex& mutex() const noexcept {
        return *m_mtx;
    }

    bool owns_lock() const noexcept {
        return m_owned;
    }

    operator bool() const noexcept {
        return owns_lock();
    }

private:
    Mutex* m_mtx;
    bool m_owned = false;
};


template <class Mutex>
shared_lock(mutex_shared_lock<Mutex> lk) -> shared_lock<Mutex>;


} // namespace asyncpp