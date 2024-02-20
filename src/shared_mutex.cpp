#include <asyncpp/shared_mutex.hpp>

#include <mutex>


namespace asyncpp {

bool shared_mutex::exclusive_awaitable::await_ready() const noexcept {
    assert(m_owner);
    return m_owner->try_lock();
}


bool shared_mutex::shared_awaitable::await_ready() const noexcept {
    assert(m_owner);
    return m_owner->try_lock_shared();
}


locked_mutex<shared_mutex> shared_mutex::exclusive_awaitable::await_resume() const noexcept {
    assert(m_owner);
    return { m_owner };
}


locked_mutex_shared<shared_mutex> shared_mutex::shared_awaitable::await_resume() const noexcept {
    assert(m_owner);
    return { m_owner };
}


shared_mutex::~shared_mutex() {
    std::lock_guard lk(m_spinlock);
    // Mutex must be released before destroying.
    if (!m_queue.empty()) {
        std::terminate();
    }
}

bool shared_mutex::try_lock() noexcept {
    std::lock_guard lk(m_spinlock);
    if (m_queue.empty()) {
        m_queue.push_back(&m_exclusive_sentinel);
        return true;
    }
    return false;
}


bool shared_mutex::try_lock_shared() noexcept {
    std::lock_guard lk(m_spinlock);
    if (m_queue.empty()) {
        m_queue.push_back(&m_shared_sentinel);
        ++m_shared_count;
        return true;
    }
    if (m_queue.back() == &m_shared_sentinel) {
        ++m_shared_count;
        return true;
    }
    return false;
}


shared_mutex::exclusive_awaitable shared_mutex::exclusive() noexcept {
    return exclusive_awaitable{ this };
}


shared_mutex::shared_awaitable shared_mutex::shared() noexcept {
    return shared_awaitable{ this };
}


bool shared_mutex::add_awaiting(basic_awaitable* waiting) {
    std::lock_guard lk(m_spinlock);
    const auto previous = m_queue.push_back(waiting);
    if (waiting->m_type == awaitable_type::exclusive) {
        // We've just acquire the exclusive lock.
        if (previous == nullptr) {
            m_queue.push_back(&m_exclusive_sentinel);
            m_queue.pop_front();
            return true;
        }
    }
    else if (waiting->m_type == awaitable_type::shared) {
        // We've just acquire the exclusive lock.
        if (previous == nullptr) {
            m_queue.push_back(&m_shared_sentinel);
            m_queue.pop_front();
            ++m_shared_count;
            return true;
        }
        // We've just acquired the shared lock.
        if (previous == &m_shared_sentinel) {
            m_queue.pop_front(); // Pop old sentinel.
            m_queue.pop_front(); // Pop just added awaitable.
            m_queue.push_back(&m_shared_sentinel);
            ++m_shared_count;
            return true;
        }
    }
    else {
        assert(false && "improperly initialized awaiter");
    }
    return false;
}


void shared_mutex::continue_waiting(std::unique_lock<spinlock>& lk) {
    decltype(m_queue) unblocked;

    const auto type = m_queue.front() ? m_queue.front()->m_type : awaitable_type::unknown;
    if (type == awaitable_type::exclusive) {
        assert(!m_queue.empty()); // Must not be empty as the front is exclusive.
        unblocked.push_back(m_queue.pop_front());
    }
    else if (type == awaitable_type::shared) {
        while (m_queue.front() && m_queue.front()->m_type == awaitable_type::shared) {
            unblocked.push_back(m_queue.pop_front());
        }
    }

    lk.unlock();
    while (const auto waiting = unblocked.pop_front()) {
        assert(waiting->m_enclosing);
        waiting->m_enclosing->resume();
    }
}


void shared_mutex::unlock() {
    std::unique_lock lk(m_spinlock);
    assert(m_queue.front() == &m_exclusive_sentinel);
    m_queue.pop_front();
    continue_waiting(lk);
}


void shared_mutex::unlock_shared() {
    std::unique_lock lk(m_spinlock);
    assert(m_queue.front() == &m_shared_sentinel);
    if (--m_shared_count == 0) {
        m_queue.pop_front();
        continue_waiting(lk);
    }
}


void shared_mutex::_debug_clear() noexcept {
    while (m_queue.pop_front()) {
    }
    m_shared_count = 0;
}


bool shared_mutex::_debug_is_exclusive_locked() const noexcept {
    return m_queue.front() == &m_exclusive_sentinel;
}


size_t shared_mutex::_debug_is_shared_locked() const noexcept {
    return m_queue.front() == &m_shared_sentinel ? m_shared_count : 0;
}

} // namespace asyncpp
