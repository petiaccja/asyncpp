#include <asyncpp/semaphore.hpp>

#include <mutex>


namespace asyncpp {


bool counting_semaphore::awaitable::await_ready() const noexcept {
    assert(m_owner);
    return m_owner->try_acquire();
}


counting_semaphore::counting_semaphore(ptrdiff_t current_counter, ptrdiff_t max_counter) noexcept : m_counter(current_counter), m_max(max_counter) {}
bool counting_semaphore::try_acquire() noexcept {
    std::lock_guard lk(m_spinlock);
    if (m_counter > 0) {
        --m_counter;
        return true;
    }
    return false;
}


counting_semaphore::awaitable counting_semaphore::operator co_await() noexcept {
    return { this };
}


void counting_semaphore::release() noexcept {
    std::unique_lock lk(m_spinlock);
    ++m_counter;
    const auto resumed = m_awaiters.pop_front();
    if (resumed) {
        --m_counter;
        lk.unlock();
        assert(resumed->m_enclosing);
        resumed->m_enclosing->resume();
    }
    else {
        if (m_counter > m_max) {
            std::terminate(); // You released the semaphore too many times.
        }
    }
}


ptrdiff_t counting_semaphore::max() const noexcept {
    return m_max;
}


ptrdiff_t counting_semaphore::_debug_get_counter() const noexcept {
    return m_counter;
}


deque<counting_semaphore::awaitable, &counting_semaphore::awaitable::m_prev, &counting_semaphore::awaitable::m_next>& counting_semaphore::_debug_get_awaiters() {
    return m_awaiters;
}


void counting_semaphore::_debug_clear() {
    m_awaiters.~deque();
    new (&m_awaiters) decltype(m_awaiters);
    m_counter = m_max;
}


bool counting_semaphore::acquire(awaitable* waiting) noexcept {
    std::lock_guard lk(m_spinlock);
    if (m_counter > 0) {
        --m_counter;
        return true;
    }
    else {
        m_awaiters.push_back(waiting);
        return false;
    }
}


} // namespace asyncpp
