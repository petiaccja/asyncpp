#pragma once

#include <atomic>


namespace asyncpp {

template <class Element, Element* Element::*next>
class atomic_list {
public:
    Element* push(Element* element) noexcept {
        Element* expected = nullptr;
        do {
            element->*next = expected;
        } while (!m_first.compare_exchange_weak(expected, element));
        m_approx_size.fetch_add(1);
        return expected;
    }

    Element* pop() noexcept {
        Element* first = nullptr;
        Element* new_first;
        do {
            new_first = first ? first->*next : nullptr;
        } while (!m_first.compare_exchange_weak(first, new_first));
        if (first) {
            m_approx_size.fetch_sub(1);
        }
        return first;
    }

    bool empty() const noexcept {
        return m_first.load() == nullptr;
    }

    size_t approx_size() const {
        return m_approx_size.load();
    }

private:
    std::atomic<Element*> m_first = nullptr;
    std::atomic_ptrdiff_t m_approx_size = 0;
};

} // namespace asyncpp