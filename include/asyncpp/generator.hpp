#pragma once

#include "promise.hpp"

#include <cassert>
#include <coroutine>
#include <exception>


namespace asyncpp {


template <class T, class Alloc>
class generator;


namespace impl_generator {

    template <class T, class Alloc>
    struct promise : allocator_aware_promise<Alloc> {
        auto get_return_object() noexcept {
            return generator<T, Alloc>(this);
        }

        constexpr auto initial_suspend() const noexcept {
            return std::suspend_never{};
        }

        constexpr auto final_suspend() const noexcept {
            return std::suspend_always{};
        }

        void unhandled_exception() noexcept {
            m_result = std::current_exception();
        }

        void return_void() noexcept {}

        auto yield_value(T value) noexcept {
            m_result = std::forward<T>(value);
            return std::suspend_always{};
        }

        task_result<T>& get_result() {
            return m_result;
        }

    private:
        task_result<T> m_result;
    };

    template <class T, class Alloc>
    class iterator {
    public:
        using promise_type = promise<T, Alloc>;
        using value_type = std::remove_reference_t<T>;
        using difference_type = ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type&;
        using iterator_category = std::input_iterator_tag;

        iterator() = default;
        explicit iterator(promise_type* promise) : m_promise(promise) {}

        reference operator*() const {
            assert(dereferenceable() && "iterator not dereferencable");
            return m_promise->get_result().get_or_throw();
        }

        iterator& operator++() {
            assert(incrementable() && "iterator not incrementable");
            m_promise->get_result().clear();
            if (!get_handle().done()) {
                get_handle().resume();
            }
            return *this;
        }

        void operator++(int) {
            ++*this;
        }

        bool operator==(const iterator& rhs) const noexcept {
            return (m_promise == rhs.m_promise) || (!incrementable() && !rhs.incrementable());
        }

        bool operator!=(const iterator& rhs) const noexcept {
            return !operator==(rhs);
        }

    private:
        bool dereferenceable() const noexcept {
            return m_promise->get_result().has_value();
        }

        bool incrementable() const noexcept {
            return m_promise && m_promise->get_result().has_value();
        }

        auto get_handle() const noexcept {
            return std::coroutine_handle<promise_type>::from_promise(*m_promise);
        }

    private:
        promise_type* m_promise = nullptr;
    };

    static_assert(std::input_iterator<iterator<int, void>>);

} // namespace impl_generator


template <class T, class Alloc = void>
class [[nodiscard]] generator {
public:
    using promise_type = impl_generator::promise<T, Alloc>;
    using iterator = impl_generator::iterator<T, Alloc>;

    generator(promise_type* promise) : m_promise(promise) {}
    generator() = default;
    generator(generator&& rhs) noexcept : m_promise(std::exchange(rhs.m_promise, nullptr)) {}
    generator& operator=(generator&& rhs) noexcept {
        release();
        m_promise = std::exchange(rhs.m_promise, nullptr);
        return *this;
    }
    generator(const generator&) = delete;
    generator& operator=(const generator&) = delete;
    ~generator() {
        release();
    }

    iterator begin() const { return iterator{ m_promise }; }
    iterator end() const { return iterator{ nullptr }; }
    iterator cbegin() const { return begin(); }
    iterator cend() const { return end(); }

private:
    void release() {
        if (m_promise) {
            std::coroutine_handle<promise_type>::from_promise(*m_promise).destroy();
        }
    }

private:
    promise_type* m_promise = nullptr;
};


template <class T>
auto begin(const generator<T>& g) { return g.begin(); }

template <class T>
auto end(const generator<T>& g) { return g.end(); }

template <class T>
auto cbegin(const generator<T>& g) { return g.begin(); }

template <class T>
auto cend(const generator<T>& g) { return g.end(); }


} // namespace asyncpp