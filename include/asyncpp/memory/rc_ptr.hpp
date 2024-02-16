#pragma once

#include <atomic>
#include <cassert>
#include <concepts>
#include <utility>


namespace asyncpp {

class rc_from_this {
    template <class T, class Deleter>
    friend class rc_ptr;

    std::atomic_size_t m_rc = 0;
};


template <class T>
struct rc_default_delete {
    void operator()(T* object) const {
        object->destroy();
    }
};


template <class T, class Deleter = rc_default_delete<T>>
class rc_ptr {
public:
    rc_ptr() noexcept(std::is_nothrow_constructible_v<Deleter>)
        : m_ptr(nullptr) {}

    explicit rc_ptr(T* ptr, Deleter deleter = {}) noexcept(std::is_nothrow_move_constructible_v<Deleter>)
        : m_ptr(ptr),
          m_deleter(std::move(deleter)) {
        increment();
    }

    rc_ptr(rc_ptr&& other) noexcept(std::is_nothrow_move_constructible_v<Deleter>)
        : m_ptr(std::exchange(other.m_ptr, nullptr)),
          m_deleter(std::move(other.m_deleter)) {}

    rc_ptr(const rc_ptr& other) noexcept(std::is_nothrow_copy_constructible_v<Deleter>)
        : m_ptr(other.m_ptr),
          m_deleter(other.m_deleter) {
        increment();
    }

    rc_ptr& operator=(rc_ptr&& other) noexcept(std::is_nothrow_move_constructible_v<Deleter>) {
        m_ptr = std::exchange(other.m_ptr, nullptr);
        m_deleter = std::move(other.m_deleter);
        return *this;
    }

    rc_ptr& operator=(const rc_ptr& other) {
        if (this != &other) {
            decrement();
            m_ptr = other.m_ptr;
            m_deleter = other.m_deleter;
            increment();
        }
        return *this;
    }

    ~rc_ptr() {
        decrement();
    }

    void reset(T* ptr = nullptr) {
        decrement();
        m_ptr = ptr;
        increment();
    }

    T* get() const noexcept {
        return m_ptr;
    }

    T& operator*() const noexcept {
        assert(m_ptr);
        return *m_ptr;
    }

    T* operator->() const noexcept {
        assert(m_ptr);
        return m_ptr;
    }

    size_t use_count() const noexcept {
        if (m_ptr) {
            return m_ptr->rc_from_this::m_rc.load(std::memory_order_relaxed);
        }
        return 0;
    }

    bool unique() const noexcept {
        return use_count() == 1;
    }

    explicit operator bool() const noexcept {
        return m_ptr != nullptr;
    }

    auto operator<=>(const rc_ptr& other) const noexcept {
        return m_ptr <=> other.m_ptr;
    }

    auto operator==(const rc_ptr& other) const noexcept {
        return m_ptr == other.m_ptr;
    }

private:
    void increment() const noexcept {
        if (m_ptr) {
            m_ptr->rc_from_this::m_rc.fetch_add(1, std::memory_order_relaxed);
        }
    }

    void decrement() const {
        if (m_ptr) {
            const auto count = m_ptr->rc_from_this::m_rc.fetch_sub(1, std::memory_order_relaxed) - 1;
            if (count == 0) {
                m_deleter(m_ptr);
            }
        }
    }

private:
    T* m_ptr = nullptr;
    Deleter m_deleter;
};

} // namespace asyncpp