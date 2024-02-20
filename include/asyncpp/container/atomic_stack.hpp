#pragma once

#include "../threading/spinlock.hpp"

#include <mutex>
#include <utility>


namespace asyncpp {


template <class Element, Element* Element::*next>
class stack {
public:
    Element* push(Element* element) noexcept {
        element->*next = m_top;
        return std::exchange(m_top, element);
    }

    Element* pop() noexcept {
        const auto new_top = m_top ? m_top->*next : nullptr;
        return std::exchange(m_top, new_top);
    }

    Element* top() const noexcept {
        return m_top;
    }

    bool empty() const noexcept {
        return m_top == nullptr;
    }

private:
    Element* m_top = nullptr;
};


template <class Element, Element* Element::*next>
class atomic_stack {
public:
    Element* push(Element* element) noexcept {
        std::lock_guard lk(m_mutex);
        return m_container.push(element);
    }

    Element* pop() noexcept {
        std::lock_guard lk(m_mutex);
        return m_container.pop();
    }

    Element* top() const noexcept {
        std::lock_guard lk(m_mutex);
        return m_container.top();
    }

    bool empty() const noexcept {
        std::lock_guard lk(m_mutex);
        return m_container.empty();
    }

private:
    stack<Element, next> m_container;
    mutable spinlock m_mutex;
};

} // namespace asyncpp