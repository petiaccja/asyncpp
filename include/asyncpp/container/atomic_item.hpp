#pragma once

#include <atomic>
#include <limits>


namespace asyncpp {

template <class Element>
class atomic_item {
public:
    atomic_item() noexcept = default;

    Element* set(Element* element) noexcept {
        return m_item.exchange(element);
    }

    Element* close() noexcept {
        return m_item.exchange(CLOSED);
    }

    bool empty() const noexcept {
        const auto item = m_item.load(std::memory_order_relaxed);
        return item == nullptr || closed(item);
    }

    bool closed() const noexcept {
        return closed(m_item.load(std::memory_order_relaxed));
    }

    static bool closed(Element* element) {
        return element == CLOSED;
    }

    Element* item() const noexcept {
        return m_item.load(std::memory_order_relaxed);
    }

private:
    std::atomic<Element*> m_item = nullptr;
    static inline Element* const CLOSED = reinterpret_cast<Element*>(std::numeric_limits<size_t>::max());
};


} // namespace asyncpp