#pragma once

#include "../threading/spinlock.hpp"

#include <atomic>
#include <mutex>


namespace asyncpp {

template <class Element, Element* Element::*next, Element* Element::*prev>
class atomic_queue {
public:
    Element* push(Element* element) noexcept {
        std::lock_guard lk(m_mtx);
        const auto prev_front = m_front.load(std::memory_order_relaxed);
        element->*prev = prev_front;
        element->*next = nullptr;
        m_front.store(element, std::memory_order_relaxed);
        if (prev_front == nullptr) {
            m_back.store(element, std::memory_order_relaxed);
        }
        else {
            prev_front->*next = element;
        }
        return prev_front;
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
            else {
                new_back->*prev = nullptr;
            }
        }
        return prev_back;
    }

    Element* front() {
        return m_front.load(std::memory_order_relaxed);
    }

    Element* back() {
        return m_back.load(std::memory_order_relaxed);
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