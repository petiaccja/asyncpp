#pragma once

#include <cassert>
#include <coroutine>
#include <mutex>
#include <utility>


namespace asyncpp {


template <class Mutex, bool Shared>
class basic_locked_mutex {
    friend Mutex;

public:
    basic_locked_mutex(basic_locked_mutex&&) = default;
    basic_locked_mutex& operator=(basic_locked_mutex&&) = default;
    basic_locked_mutex(const basic_locked_mutex&) = delete;
    basic_locked_mutex& operator=(const basic_locked_mutex&) = delete;
    Mutex& mutex() const noexcept {
        return *m_mtx;
    }

    static constexpr bool shared() noexcept {
        return Shared;
    }

private:
    basic_locked_mutex(Mutex* mtx) : m_mtx(mtx) {}
    Mutex* m_mtx = nullptr;
};


template <class Mutex>
using exclusively_locked_mutex = basic_locked_mutex<Mutex, false>;
template <class Mutex>
using shared_locked_mutex = basic_locked_mutex<Mutex, true>;


template <class Mutex, bool Shared, auto Lock, bool (Mutex::*TryLock)(), void (Mutex::*Unlock)()>
class basic_lock {
    // NOTE: GCC bugs out on `auto (Mutex::*Lock)()`, that's why `Lock` is simply `auto`.

    using mutex_awaitable_t = std::invoke_result_t<decltype(Lock), Mutex*>;

    struct awaitable {
        basic_lock* m_owner;
        mutex_awaitable_t m_impl;

        auto await_ready() noexcept {
            return m_impl.await_ready();
        }

        template <class Promise>
        auto await_suspend(std::coroutine_handle<Promise> enclosing) noexcept {
            return m_impl.await_suspend(enclosing);
        }

        void await_resume() noexcept {
            m_impl.await_resume();
            m_owner->m_owned = true;
        }
    };

public:
    basic_lock(Mutex& mtx, std::defer_lock_t) noexcept : m_mtx(&mtx), m_owned(false) {}
    basic_lock(Mutex& mtx, std::adopt_lock_t) noexcept : m_mtx(&mtx), m_owned(true) {}
    basic_lock(basic_locked_mutex<Mutex, Shared>&& lk) noexcept : m_mtx(&lk.mutex()), m_owned(true) {}
    basic_lock(basic_lock&& rhs) noexcept : m_mtx(rhs.m_mtx), m_owned(rhs.m_owned) {
        rhs.m_mtx = nullptr;
        rhs.m_owned = false;
    }
    basic_lock& operator=(basic_lock&& rhs) noexcept {
        if (owns_lock()) {
            (m_mtx->*Unlock)();
        }
        m_mtx = std::exchange(rhs.m_mtx, nullptr);
        m_owned = std::exchange(rhs.m_owned, false);
        return *this;
    }
    basic_lock(const basic_lock& rhs) = delete;
    basic_lock& operator=(const basic_lock& rhs) = delete;
    ~basic_lock() {
        if (owns_lock()) {
            (m_mtx->*Unlock)();
        }
    }

    bool try_lock() noexcept {
        assert(!owns_lock());
        m_owned = (m_mtx->*TryLock)();
        return m_owned;
    }

    auto operator co_await() noexcept {
        assert(!owns_lock());
        return awaitable{ this, (m_mtx->*Lock)() };
    }

    void unlock() noexcept {
        assert(owns_lock());
        (m_mtx->*Unlock)();
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
class unique_lock : public basic_lock<Mutex, false, &Mutex::exclusive, &Mutex::try_lock, &Mutex::unlock> {
    using basic_lock<Mutex, false, &Mutex::exclusive, &Mutex::try_lock, &Mutex::unlock>::basic_lock;
};

template <class Mutex>
class shared_lock : public basic_lock<Mutex, true, &Mutex::shared, &Mutex::try_lock_shared, &Mutex::unlock_shared> {
    using basic_lock<Mutex, true, &Mutex::shared, &Mutex::try_lock_shared, &Mutex::unlock_shared>::basic_lock;
};


template <class Mutex>
unique_lock(exclusively_locked_mutex<Mutex>) -> unique_lock<Mutex>;
template <class Mutex>
unique_lock(Mutex&, std::adopt_lock_t) -> unique_lock<Mutex>;
template <class Mutex>
unique_lock(Mutex&, std::defer_lock_t) -> unique_lock<Mutex>;


template <class Mutex>
shared_lock(shared_locked_mutex<Mutex>) -> shared_lock<Mutex>;
template <class Mutex>
shared_lock(Mutex&, std::adopt_lock_t) -> shared_lock<Mutex>;
template <class Mutex>
shared_lock(Mutex&, std::defer_lock_t) -> shared_lock<Mutex>;


} // namespace asyncpp