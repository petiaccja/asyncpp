#pragma once

#include <concepts>


namespace asyncpp {

class scheduler;

// clang-format off

template <class T>
concept directly_awaitable = requires(std::remove_reference_t<T>& t) {
    { t.await_ready() } -> std::convertible_to<bool>;
    { t.await_resume() };
};


template <class T>
concept indirectly_awaitable = requires(std::remove_reference_t<T>& t) {
    { t.operator co_await() } -> directly_awaitable;
};


template <class T>
concept awaitable = directly_awaitable<T> || indirectly_awaitable<T>;


template <class T>
concept launchable_coroutine = requires(T&& t) { t.launch(); };


template <class T>
concept bindable_coroutine = requires(T&& t, scheduler& s) { t.bind(s); };


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