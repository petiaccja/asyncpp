#pragma once

#include "../threading/spinlock.hpp"

#include <mutex>


namespace asyncpp {

template <class Element, Element* Element::*prev, Element* Element::*next>
class deque {
public:
    Element* push_front(Element* element) noexcept {
        element->*prev = nullptr;
        element->*next = m_front;
        if (m_front) {
            m_front->*prev = element;
        }
        else {
            m_back = element;
        }
        return std::exchange(m_front, element);
    }

    Element* push_back(Element* element) noexcept {
        element->*prev = m_back;
        element->*next = nullptr;
        if (m_back) {
            m_back->*next = element;
        }
        else {
            m_front = element;
        }
        return std::exchange(m_back, element);
    }

    Element* pop_front() noexcept {
        if (m_front) {
            const auto element = std::exchange(m_front, m_front->*next);
            if (m_front) {
                m_front->*prev = nullptr;
            }
            else {
                m_back = nullptr;
            }
            element->*next = nullptr;
            return element;
        }
        return nullptr;
    }

    Element* pop_back() noexcept {
        if (m_back) {
            const auto element = std::exchange(m_back, m_back->*prev);
            if (m_back) {
                m_back->*next = nullptr;
            }
            else {
                m_front = nullptr;
            }
            element->*prev = nullptr;
            return element;
        }
        return nullptr;
    }

    Element* front() const noexcept {
        return m_front;
    }

    Element* back() const noexcept {
        return m_back;
    }

private:
    Element* m_front = nullptr;
    Element* m_back = nullptr;
};


template <class Element, Element* Element::*prev, Element* Element::*next>
class atomic_deque {
public:
    Element* push_front(Element* element) noexcept {
        std::lock_guard lk(m_mutex);
        return m_container.push_front(element);
    }

    Element* push_back(Element* element) noexcept {
        std::lock_guard lk(m_mutex);
        return m_container.push_back(element);
    }

    Element* pop_front() noexcept {
        std::lock_guard lk(m_mutex);
        return m_container.pop_front();
    }

    Element* pop_back() noexcept {
        std::lock_guard lk(m_mutex);
        return m_container.pop_back();
    }

    Element* front() const noexcept {
        std::lock_guard lk(m_mutex);
        return m_container.front();
    }

    Element* back() const noexcept {
        std::lock_guard lk(m_mutex);
        return m_container.back();
    }

private:
    deque<Element, prev, next> m_container;
    mutable spinlock m_mutex;
};

} // namespace asyncpp