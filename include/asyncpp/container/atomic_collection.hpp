#pragma once

#include <atomic>
#include <limits>


namespace asyncpp {

template <class Element, Element* Element::*next>
class atomic_collection {
public:
    atomic_collection() noexcept = default;

    Element* push(Element* element) noexcept {
        Element* first = nullptr;
        do {
            element->*next = first;
        } while (first != CLOSED && !m_first.compare_exchange_weak(first, element));
        return first;
    }

    Element* detach() noexcept {
        return m_first.exchange(nullptr);
    }

    Element* close() noexcept {
        return m_first.exchange(CLOSED);
    }

    bool empty() const noexcept {
        const auto item = m_first.load(std::memory_order_relaxed);
        return item == nullptr || closed(item);
    }

    bool closed() const noexcept {
        return closed(m_first.load(std::memory_order_relaxed));
    }

    static bool closed(Element* element) {
        return element == CLOSED;
    }

    Element* first() const noexcept {
        return m_first.load(std::memory_order_relaxed);
    }

private:
    std::atomic<Element*> m_first = nullptr;
    static inline Element* const CLOSED = reinterpret_cast<Element*>(std::numeric_limits<size_t>::max());
};

} // namespace asyncpp