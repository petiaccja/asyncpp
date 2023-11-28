#pragma once

#include "awaitable.hpp"

#include <coroutine>


namespace asyncpp {

class scheduler;


template <class T>
struct task_result {
    using wrapper_type = std::conditional_t<std::is_reference_v<T>, std::reference_wrapper<std::remove_reference_t<T>>, T>;
    using value_type = std::conditional_t<std::is_void_v<T>, std::nullptr_t, wrapper_type>;
    using reference = std::conditional_t<std::is_void_v<T>, void, std::add_lvalue_reference_t<T>>;

    std::optional<std::variant<value_type, std::exception_ptr>> m_result;

    task_result() = default;

    task_result(value_type value) : m_result(std::move(value)) {}

    task_result(std::exception_ptr value) : m_result(std::move(value)) {}

    task_result& operator=(value_type value) {
        m_result = std::move(value);
        return *this;
    }

    task_result& operator=(std::exception_ptr value) {
        m_result = std::move(value);
        return *this;
    }

    void clear() {
        m_result = std::nullopt;
    }

    bool has_value() const {
        return m_result.has_value();
    }

    reference get_or_throw() {
        auto& value = m_result.value(); // Throws if empty.
        if (std::holds_alternative<std::exception_ptr>(value)) {
            std::rethrow_exception(std::get<std::exception_ptr>(value));
        }
        return static_cast<reference>(std::get<value_type>(value));
    }
};


struct resumable_promise {
    virtual ~resumable_promise() = default;
    virtual void resume() = 0;
};


struct schedulable_promise {
    virtual ~schedulable_promise() = default;
    virtual std::coroutine_handle<> handle() = 0;
    schedulable_promise* m_scheduler_next = nullptr;
    scheduler* m_scheduler = nullptr;
};


template <class T>
struct return_promise {
    task_result<T> m_result;

    void unhandled_exception() noexcept {
        m_result = std::current_exception();
    }
    void return_value(T value) noexcept {
        m_result = std::forward<T>(value);
    }
};


template <>
struct return_promise<void> {
    task_result<void> m_result;

    void unhandled_exception() noexcept {
        m_result = std::current_exception();
    }
    void return_void() noexcept {
        m_result = nullptr;
    }
};

} // namespace asyncpp