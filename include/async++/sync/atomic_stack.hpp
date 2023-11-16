#pragma once

#include "spinlock.hpp"

#include <mutex>


namespace asyncpp {

template <class Element, Element* Element::*next>
class atomic_stack {
public:
    Element* push(Element* element) noexcept {
        std::lock_guard lk(m_mtx);
        const auto prev_first = m_first.load(std::memory_order_relaxed);
        element->*next = prev_first;
        m_first = element;
        return prev_first;
    }

    Element* pop() noexcept {
        std::lock_guard lk(m_mtx);
        const auto prev_first = m_first.load(std::memory_order_relaxed);
        if (prev_first != nullptr) {
            m_first = prev_first->*next;
        }
        return prev_first;
    }

    bool empty() const noexcept {
        return m_first.load(std::memory_order_relaxed) == nullptr;
    }

private:
    std::atomic<Element*> m_first = nullptr;
    mutable spinlock m_mtx;
};

} // namespace asyncpp