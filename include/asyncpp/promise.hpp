#pragma once

#include <algorithm>
#include <coroutine>
#include <cstddef>
#include <memory>
#include <optional>
#include <utility>
#include <variant>


namespace asyncpp {

class scheduler;


template <class T>
struct task_result {
    using wrapper_type = std::conditional_t<std::is_reference_v<T>, std::reference_wrapper<std::remove_reference_t<T>>, T>;
    using value_type = std::conditional_t<std::is_void_v<T>, std::nullptr_t, wrapper_type>;
    using reference = std::conditional_t<std::is_void_v<T>, void, std::add_lvalue_reference_t<T>>;

    std::optional<std::variant<value_type, std::exception_ptr>> m_result;

    task_result() = default;
    explicit task_result(value_type value) : m_result(std::move(value)) {}
    explicit task_result(std::exception_ptr value) : m_result(std::move(value)) {}

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

    value_type move_or_throw() {
        auto& value = m_result.value(); // Throws if empty.
        if (std::holds_alternative<std::exception_ptr>(value)) {
            std::rethrow_exception(std::get<std::exception_ptr>(value));
        }
        return std::move(std::get<value_type>(value));
    }

    auto operator<=>(const task_result&) const noexcept = default;
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
struct result_promise {
    task_result<T> m_result;

    void unhandled_exception() noexcept {
        m_result = std::current_exception();
    }
    void return_value(T value) noexcept {
        m_result = std::forward<T>(value);
    }
};


template <>
struct result_promise<void> {
    task_result<void> m_result;

    void unhandled_exception() noexcept {
        m_result = std::current_exception();
    }
    void return_void() noexcept {
        m_result = nullptr;
    }
};


template <class Alloc>
struct allocator_aware_promise {
private:
    template <size_t Alignment = alignof(std::max_align_t)>
    struct aligned_block {
        alignas(Alignment) std::byte memory[Alignment];
    };

    using dealloc_t = void (*)(void*, size_t);

    static constexpr auto dealloc_offset(size_t size) {
        constexpr auto dealloc_alignment = alignof(dealloc_t);
        return (size + dealloc_alignment - 1) / dealloc_alignment * dealloc_alignment;
    }

    template <class Alloc_, class... Args>
        requires std::convertible_to<Alloc_, Alloc> || std::is_void_v<Alloc>
    static void* allocate(size_t size, std::allocator_arg_t, const Alloc_& alloc, Args&&...) {
        static constexpr auto alloc_alignment = alignof(Alloc_);
        static constexpr auto promise_alignment = std::max({ alignof(std::max_align_t), alignof(Args)... });
        static constexpr auto alignment = std::max(alloc_alignment, promise_alignment);
        using block_t = aligned_block<alignment>;
        using alloc_t = typename std::allocator_traits<Alloc_>::template rebind_alloc<block_t>;
        static_assert(alignof(alloc_t) <= alignof(Alloc_));

        static constexpr auto alloc_offset = [](size_t size) {
            const auto extended_size = dealloc_offset(size) + sizeof(dealloc_t);
            return (extended_size + alloc_alignment - 1) / alloc_alignment * alloc_alignment;
        };

        static constexpr auto total_size = [](size_t size) {
            return alloc_offset(size) + sizeof(alloc_t);
        };

        static constexpr dealloc_t dealloc = [](void* ptr, size_t size) {
            auto& alloc = *reinterpret_cast<alloc_t*>(static_cast<std::byte*>(ptr) + alloc_offset(size));
            auto moved = std::move(alloc);
            alloc.~alloc_t();
            const auto num_blocks = (total_size(size) + sizeof(block_t) - 1) / sizeof(block_t);
            std::allocator_traits<alloc_t>::deallocate(moved, static_cast<block_t*>(ptr), num_blocks);
        };

        auto rebound_alloc = alloc_t(alloc);
        const auto num_blocks = (total_size(size) + sizeof(block_t) - 1) / sizeof(block_t);
        const auto ptr = std::allocator_traits<alloc_t>::allocate(rebound_alloc, num_blocks);
        const auto dealloc_ptr = reinterpret_cast<dealloc_t*>(reinterpret_cast<std::byte*>(ptr) + dealloc_offset(size));
        const auto alloc_ptr = reinterpret_cast<alloc_t*>(reinterpret_cast<std::byte*>(ptr) + alloc_offset(size));
        new (dealloc_ptr) dealloc_t(dealloc);
        new (alloc_ptr) alloc_t(std::move(rebound_alloc));
        return ptr;
    }

public:
    template <class Alloc_, class... Args>
        requires std::convertible_to<Alloc_, Alloc> || std::is_void_v<Alloc>
    void* operator new(size_t size, std::allocator_arg_t, const Alloc_& alloc, Args&&... args) {
        return allocate(size, std::allocator_arg, alloc, std::forward<Args>(args)...);
    }

    template <class Self, class Alloc_, class... Args>
        requires std::convertible_to<Alloc_, Alloc> || std::is_void_v<Alloc>
    void* operator new(size_t size, Self&, std::allocator_arg_t, const Alloc_& alloc, Args&&... args) {
        return allocate(size, std::allocator_arg, alloc, std::forward<Args>(args)...);
    }

    template <class... Args>
        requires(... && !std::convertible_to<Args, std::allocator_arg_t>)
    void* operator new(size_t size, Args&&... args) {
        if constexpr (!std::is_void_v<Alloc>) {
            return allocate(size, std::allocator_arg, Alloc{}, std::forward<Args>(args)...);
        }
        else {
            return allocate(size, std::allocator_arg, std::allocator<std::byte>{}, std::forward<Args>(args)...);
        }
    }

    void operator delete(void* ptr, size_t size) {
        const auto dealloc_ptr = reinterpret_cast<dealloc_t*>(static_cast<std::byte*>(ptr) + dealloc_offset(size));
        (*dealloc_ptr)(ptr, size);
    }
};

} // namespace asyncpp