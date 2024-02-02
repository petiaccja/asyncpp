#include <asyncpp/mutex.hpp>

#include <mutex>


namespace asyncpp {

bool mutex::awaitable::await_ready() noexcept {
    return m_mtx->try_lock();
}


locked_mutex<mutex> mutex::awaitable::await_resume() noexcept {
    return { m_mtx };
}


void mutex::awaitable::on_ready() noexcept {
    assert(m_enclosing);
    m_enclosing->resume();
}


mutex::~mutex() {
    std::lock_guard lk(m_spinlock);
    if (m_locked) {
        std::terminate();
    }
}

bool mutex::try_lock() noexcept {
    std::lock_guard lk(m_spinlock);
    return std::exchange(m_locked, true) == false;
}


mutex::awaitable mutex::unique() noexcept {
    return { this };
}


mutex::awaitable mutex::operator co_await() noexcept {
    return unique();
}


bool mutex::lock_enqueue(awaitable* waiting) {
    std::lock_guard lk(m_spinlock);
    const bool acquired = std::exchange(m_locked, true) == false;
    if (acquired) {
        return true;
    }
    m_queue.push(waiting);
    return false;
}


void mutex::unlock() {
    std::unique_lock lk(m_spinlock);
    assert(m_locked);
    m_locked = false;
    const auto next = m_queue.pop();
    if (next) {
        m_locked = true;
    }
    lk.unlock();
    if (next) {
        next->on_ready();
    }
}

} // namespace asyncpp