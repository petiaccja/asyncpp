#pragma once

#include "../sync/spinlock.hpp"

#include <atomic>


namespace asyncpp {

template <class Element, Element* Element::*next, Element* Element::*prev>
class atomic_queue {
public:
    Element* push(Element* element) noexcept {
        std::lock_guard lk(m_mtx);
        const auto prev_front = m_front.load(std::memory_order_relaxed);
        element->*prev = prev_front;
        m_front.store(element, std::memory_order_relaxed);
        if (prev_front == nullptr) {
            m_back.store(element, std::memory_order_relaxed);
        }
        else {
            prev_front->*next = element;
        }
        return prev_front;
    }

    bool compare_push(Element*& expected, Element* element) {
        std::lock_guard lk(m_mtx);
        const auto prev_front = m_front.load(std::memory_order_relaxed);
        if (prev_front == expected) {
            element->*prev = prev_front;
            m_front.store(element, std::memory_order_relaxed);
            if (prev_front == nullptr) {
                m_back.store(element, std::memory_order_relaxed);
            }
            else {
                prev_front->*next = element;
            }
            return true;
        }
        expected = prev_front;
        return false;
    }

    Element* pop() noexcept {
        std::lock_guard lk(m_mtx);
        const auto prev_back = m_back.load(std::memory_order_relaxed);
        if (prev_back != nullptr) {
            const auto new_back = prev_back->*next;
            m_back.store(new_back, std::memory_order_relaxed);
            if (new_back == nullptr) {
                m_front.store(nullptr, std::memory_order_relaxed);
            }
        }
        return prev_back;
    }

    bool empty() const noexcept {
        return m_back.load(std::memory_order_relaxed) == nullptr;
    }

private:
    std::atomic<Element*> m_front;
    std::atomic<Element*> m_back;
    mutable spinlock m_mtx;
};

} // namespace asyncpp