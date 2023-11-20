#pragma once

#include "concepts.hpp"
#include "promise.hpp"


namespace asyncpp {

class scheduler {
public:
    virtual ~scheduler() = default;
    virtual void schedule(impl::schedulable_promise& promise) = 0;
};


template <bindable_coroutine T>
auto bind(T&& t, scheduler& s) -> decltype(auto) {
    t.bind(s);
    return std::forward<T>(t);
}


template <launchable_coroutine T>
auto launch(T&& t) -> decltype(auto) {
    t.launch();
    return std::forward<T>(t);
}


template <class T>
    requires(bindable_coroutine<T> && launchable_coroutine<T>)
auto launch(T&& t, scheduler& s) -> decltype(auto) {
    bind(t, s);
    return launch(std::forward<T>(t));
}

} // namespace asyncpp