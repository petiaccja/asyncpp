#pragma once

#include <exception>
#include <optional>
#include <variant>


namespace asyncpp {
namespace impl {

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


    template <class T>
    struct basic_awaitable {
        virtual ~basic_awaitable() = default;
        virtual void on_ready() noexcept = 0;
    };


} // namespace impl
} // namespace asyncpp