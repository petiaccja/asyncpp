#include <asyncpp/mutex.hpp>

#include <mutex>


namespace asyncpp {

bool mutex::awaitable::await_ready() const noexcept {
    assert(m_owner);
    return m_owner->try_lock();
}


exclusively_locked_mutex<mutex> mutex::awaitable::await_resume() noexcept {
    assert(m_owner);
    return { m_owner };
}


mutex::~mutex() {
    std::lock_guard lk(m_spinlock);
    // Mutex must be unlocked before it's destroyed.
    if (!m_queue.empty()) {
        std::terminate();
    }
}

bool mutex::try_lock() noexcept {
    std::lock_guard lk(m_spinlock);
    if (m_queue.empty()) {
        m_queue.push_back(&m_sentinel);
        return true;
    }
    return false;
}


mutex::awaitable mutex::exclusive() noexcept {
    return { this };
}


mutex::awaitable mutex::operator co_await() noexcept {
    return exclusive();
}


bool mutex::add_awaiting(awaitable* waiting) {
    std::lock_guard lk(m_spinlock);
    const auto previous = m_queue.push_back(waiting);
    // We've just acquired the lock.
    if (previous == nullptr) {
        m_queue.push_back(&m_sentinel);
        m_queue.pop_front();
        return true;
    }
    // We haven't acquired the lock.
    return false;
}


void mutex::unlock() {
    std::unique_lock lk(m_spinlock);
    assert(!m_queue.empty()); // Sentinel must be in the queue.
    const auto sentinel = m_queue.pop_front();
    assert(sentinel == &m_sentinel);
    const auto next = m_queue.pop_front();
    if (next != nullptr) {
        m_queue.push_front(&m_sentinel);
        lk.unlock();
        assert(next->m_enclosing);
        next->m_enclosing->resume();
    }
}


void mutex::_debug_clear() noexcept {
    m_queue.~deque();
    new (&m_queue) decltype(m_queue);
}


bool mutex::_debug_is_locked() noexcept {
    return m_queue.front() == &m_sentinel;
}

} // namespace asyncpp