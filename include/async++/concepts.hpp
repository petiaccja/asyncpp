#pragma once

#include <concepts>


namespace asyncpp {

// clang-format off

template <class T>
concept directly_awaitable = requires(T& t) {
    { t.await_ready() } -> std::convertible_to<bool>;
    { t.await_resume() };
};


template <class T>
concept indirectly_awaitable = requires(T& t) {
    { t.operator co_await() } -> directly_awaitable;
};


template <class T>
concept awaitable = directly_awaitable<T> || indirectly_awaitable<T>;

// clang-format on

template <class T>
struct await_result {};


template <directly_awaitable T>
struct await_result<T> {
    using type = decltype(std::declval<T>().await_resume());
};


template <indirectly_awaitable T>
struct await_result<T> {
    using type = decltype(std::declval<T>().operator co_await().await_resume());
};


template <class T>
using await_result_t = typename await_result<T>::type;


}; // namespace asyncpp