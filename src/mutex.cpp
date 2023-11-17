#include <async++/mutex.hpp>

namespace asyncpp {

bool mutex::awaitable::await_ready() noexcept {
    m_lk = m_mtx->try_lock();
    return m_lk.has_value();
}


mutex::lock mutex::awaitable::await_resume() noexcept {
    assert(m_lk);
    return std::move(m_lk.value());
}


void mutex::awaitable::on_ready(lock lk) noexcept {
    m_lk = std::move(lk);
    assert(m_enclosing);
    m_enclosing->resume();
}


std::optional<mutex::lock> mutex::try_lock() noexcept {
    std::lock_guard lk(m_spinlock);
    if (std::exchange(m_locked, true) == false) {
        return lock(this);
    }
    return std::nullopt;
}


mutex::awaitable mutex::unique() noexcept {
    return awaitable(this);
}


mutex::awaitable mutex::operator co_await() noexcept {
    return unique();
}


std::optional<mutex::lock> mutex::wait(awaitable* waiting) {
    std::lock_guard lk(m_spinlock);
    const bool acquired = std::exchange(m_locked, true) == false;
    if (acquired) {
        return lock(this);
    }
    m_queue.push(waiting);
    return std::nullopt;
}


void mutex::unlock() {
    std::unique_lock lk(m_spinlock);
    assert(m_locked);
    m_locked = false;
    awaitable* const next = m_queue.pop();
    lk.unlock();
    if (next) {
        m_locked = true;
        next->on_ready(lock(this));
    }
}

} // namespace asyncpp