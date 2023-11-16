#pragma once

#include "promise.hpp"


namespace asyncpp {

class scheduler {
public:
    virtual ~scheduler() = default;
    virtual void schedule(impl::schedulable_promise& promise) = 0;
};


template <class T>
    requires requires(T& t, scheduler& s) { t.bind(s); }
T& bind(T& t, scheduler& s) {
    t.bind(s);
    return t;
}


template <class T>
    requires requires(T&& t, scheduler& s) { t.bind(s); }
T bind(T&& t, scheduler& s) {
    t.bind(s);
    return std::move(t);
}


template <class T>
    requires requires(T& t, scheduler& s) { bind(t, s); t.launch(); }
T& launch(T& t, scheduler& s) {
    bind(t, s);
    t.launch();
    return t;
}


template <class T>
    requires requires(T&& t, scheduler& s) { bind(t, s); t.launch(); }
T launch(T&& t, scheduler& s) {
    bind(t, s);
    t.launch();
    return std::move(t);
}

} // namespace asyncpp