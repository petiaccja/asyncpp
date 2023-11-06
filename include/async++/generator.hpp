#pragma once

#include <cassert>
#include <coroutine>
#include <exception>
#include <optional>


namespace asyncpp {


template <class T>
class generator;


namespace impl_generator {

    template <class T>
    struct promise {
        using storage_type = std::conditional_t<std::is_reference_v<T>, std::reference_wrapper<std::remove_reference_t<T>>, T>;

        promise() = default;

        auto get_return_object() noexcept { return generator<T>(this); }
        constexpr auto initial_suspend() const noexcept { return std::suspend_never{}; }
        constexpr auto final_suspend() const noexcept { return std::suspend_always{}; }
        void unhandled_exception() noexcept { m_error = std::current_exception(); }
        void return_void() noexcept {}
        auto yield_value(storage_type value) noexcept {
            m_current_value = std::move(value);
            return std::suspend_always{};
        }

        std::optional<storage_type> m_current_value;
        std::optional<std::exception_ptr> m_error;
    };

    template <class T>
    class iterator {
    public:
        using promise_type = promise<T>;
        using value_type = std::remove_reference_t<T>;
        using difference_type = ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type&;
        using iterator_category = std::input_iterator_tag;

        iterator() = default;
        explicit iterator(promise_type* promise) : m_promise(promise) {}

        reference operator*() const {
            assert(valid() && "iterator not dereferencable");
            if (m_promise->m_error) {
                std::rethrow_exception(*m_promise->m_error);
            }
            return *m_promise->m_current_value;
        }

        iterator& operator++() {
            assert(valid() && "iterator not incrementable");
            get_handle().resume();
            return *this;
        }

        iterator operator++(int) {
            auto copy = *this;
            ++*this;
            return copy;
        }

        bool operator==(const iterator& rhs) const noexcept {
            return (m_promise == rhs.m_promise) || (!valid() && !rhs.m_promise);
        }

        bool operator!=(const iterator& rhs) const noexcept {
            return !operator==(rhs);
        }

    private:
        bool valid() const noexcept {
            return m_promise && !get_handle().done();
        }

        auto get_handle() const noexcept {
            return std::coroutine_handle<promise_type>::from_promise(*m_promise);
        }

    private:
        promise_type* m_promise = nullptr;
    };

    static_assert(std::input_iterator<iterator<int>>);

} // namespace impl_generator


template <class T>
class [[nodiscard]] generator {
public:
    using promise_type = impl_generator::promise<T>;
    using iterator = impl_generator::iterator<T>;

    generator(promise_type* promise) : m_promise(promise) {}
    generator() = default;
    generator(generator&& rhs) noexcept : m_promise(rhs.m_promise) { rhs.m_promise = nullptr; }
    generator& operator=(generator&& rhs) noexcept {
        release();
        m_promise = rhs.m_promise;
        rhs.m_promise = nullptr;
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