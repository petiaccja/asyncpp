#pragma once

#include <atomic>


namespace asyncpp {

class spinlock {
public:
    void lock() {
        while (!try_lock()) {
        }
    }

    bool try_lock() {
        const bool was_locked = m_locked.test_and_set(std::memory_order_acquire);
        return !was_locked;
    }

    void unlock() {
        m_locked.clear(std::memory_order_release);
    }

private:
    std::atomic_flag m_locked;
};

} // namespace asyncpp