#pragma once

#include "promise.hpp"


namespace asyncpp {

class scheduler {
public:
    virtual ~scheduler() = default;
    virtual void schedule(impl::schedulable_promise& promise) = 0;
};


template <class T>
    requires requires(T&& t, scheduler& s) { t.bind(s); }
auto bind(T&& t, scheduler& s) -> decltype(auto) {
    t.bind(s);
    return std::forward<T>(t);
}


template <class T>
    requires requires(T&& t) { t.launch(); }
auto launch(T&& t) -> decltype(auto) {
    t.launch();
    return std::forward<T>(t);
}


template <class T>
    requires requires(T&& t, scheduler& s) { bind(t, s); launch(t); }
auto launch(T&& t, scheduler& s) -> decltype(auto) {
    bind(t, s);
    return launch(std::forward<T>(t));
}

} // namespace asyncpp