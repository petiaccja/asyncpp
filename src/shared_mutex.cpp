#include <asyncpp/shared_mutex.hpp>

#include <mutex>


namespace asyncpp {

bool shared_mutex::awaitable::await_ready() noexcept {
    return m_mtx->try_lock();
}


locked_mutex<shared_mutex> shared_mutex::awaitable::await_resume() noexcept {
    return { m_mtx };
}


void shared_mutex::awaitable::on_ready() noexcept {
    assert(m_enclosing);
    m_enclosing->resume();
}


bool shared_mutex::awaitable::is_shared() const noexcept {
    return false;
}


bool shared_mutex::shared_awaitable::await_ready() noexcept {
    return m_mtx->try_lock_shared();
}


locked_mutex_shared<shared_mutex> shared_mutex::shared_awaitable::await_resume() noexcept {
    return { m_mtx };
}


void shared_mutex::shared_awaitable::on_ready() noexcept {
    assert(m_enclosing);
    m_enclosing->resume();
}


bool shared_mutex::shared_awaitable::is_shared() const noexcept {
    return true;
}


shared_mutex::~shared_mutex() {
    std::lock_guard lk(m_spinlock);
    if (m_locked != 0) {
        std::terminate();
    }
}

bool shared_mutex::try_lock() noexcept {
    std::lock_guard lk(m_spinlock);
    if (m_locked == 0) {
        --m_locked;
        return true;
    }
    return false;
}


bool shared_mutex::try_lock_shared() noexcept {
    std::lock_guard lk(m_spinlock);
    if (m_locked >= 0 && m_unique_waiting == 0) {
        ++m_locked;
        return true;
    }
    return false;
}


shared_mutex::awaitable shared_mutex::unique() noexcept {
    return { this };
}


shared_mutex::shared_awaitable shared_mutex::shared() noexcept {
    return { this };
}


bool shared_mutex::lock_enqueue(awaitable* waiting) {
    std::lock_guard lk(m_spinlock);
    if (m_locked == 0) {
        --m_locked;
        return true;
    }
    m_queue.push(waiting);
    ++m_unique_waiting;
    return false;
}


bool shared_mutex::lock_enqueue_shared(shared_awaitable* waiting) {
    std::lock_guard lk(m_spinlock);
    if (m_locked >= 0 && m_unique_waiting == 0) {
        ++m_locked;
        return true;
    }
    m_queue.push(waiting);
    return false;
}


void shared_mutex::unlock() {
    std::unique_lock lk(m_spinlock);
    assert(m_locked == -1);
    ++m_locked;
    queue_t next_list;
    basic_awaitable* next;
    do {
        next = m_queue.pop();
        if (next) {
            m_locked += next->is_shared() ? +1 : -1;
            m_unique_waiting -= intptr_t(!next->is_shared());
            next_list.push(next);
        }
    } while (next && next->is_shared() && !m_queue.empty() && m_queue.back()->is_shared() && m_locked >= 0);
    lk.unlock();
    while ((next = next_list.pop()) != nullptr) {
        next->on_ready();
    }
}


void shared_mutex::unlock_shared() {
    std::unique_lock lk(m_spinlock);
    assert(m_locked > 0);
    --m_locked;
    if (m_locked == 0) {
        const auto next = m_queue.pop();
        if (next) {
            assert(!next->is_shared()); // Shared ones would have been continued immediately.
            --m_locked;
            --m_unique_waiting;
            lk.unlock();
            next->on_ready();
        }
    }
}

} // namespace asyncpp
