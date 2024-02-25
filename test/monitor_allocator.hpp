#pragma once

#include <atomic>
#include <memory>


namespace impl_monitor_allocator {

struct counters {
    std::atomic_size_t num_allocations;
    std::atomic_size_t num_deallocations;
};

} // namespace impl_monitor_allocator


template <class T = std::byte>
class monitor_allocator {
public:
    using value_type = T;

    template <class U>
    friend class monitor_allocator;

    constexpr monitor_allocator()
        : m_counters(std::make_shared<impl_monitor_allocator::counters>()) {}

    constexpr monitor_allocator(const monitor_allocator& other) noexcept = default;

    template <class U>
    constexpr monitor_allocator(const monitor_allocator<U>& other) noexcept
        : m_counters(other.m_counters) {}

    T* allocate(size_t n) {
        m_counters->num_allocations.fetch_add(1, std::memory_order_relaxed);
        return std::allocator<T>().allocate(n);
    }

    void deallocate(T* ptr, size_t n) {
        m_counters->num_deallocations.fetch_add(1, std::memory_order_relaxed);
        return std::allocator<T>().deallocate(ptr, n);
    }

    size_t get_num_allocations() const {
        return m_counters->num_allocations.load(std::memory_order_relaxed);
    }

    size_t get_num_deallocations() const {
        return m_counters->num_allocations.load(std::memory_order_relaxed);
    }

    size_t get_num_live_objects() const {
        return get_num_allocations() - get_num_deallocations();
    }

private:
    std::shared_ptr<impl_monitor_allocator::counters> m_counters;
};