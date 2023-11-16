#pragma once


#include <atomic>
#include <limits>


namespace asyncpp {

template <class Element, Element* Element::*next>
class atomic_collection {
public:
    atomic_collection() = default;

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
        return m_first.load(std::memory_order_relaxed) == nullptr || closed();
    }

    bool closed() const noexcept {
        return m_first.load(std::memory_order_relaxed) == CLOSED;
    }

    static bool closed(Element* element) {
        return element == CLOSED;
    }

    Element* first() {
        return m_first.load(std::memory_order_relaxed);
    }

    const Element* first() const {
        return m_first.load(std::memory_order_relaxed);
    }

protected:
    atomic_collection(Element* first) : m_first(first) {}

private:
    std::atomic<Element*> m_first = nullptr;
    static inline Element* const CLOSED = reinterpret_cast<Element*>(std::numeric_limits<size_t>::max());
};

} // namespace asyncpp