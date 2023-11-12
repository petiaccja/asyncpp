#pragma once

#include "schedulable.hpp"


namespace asyncpp {

class scheduler {
public:
    virtual ~scheduler() = default;
    virtual void schedule(impl::schedulable_promise& promise) = 0;
};


template <class T>
    requires requires(T& t, scheduler& s) { t.set_scheduler(s); }
T& set_scheduler(T& t, scheduler& s) {
    t.set_scheduler(s);
    return t;
}


template <class T>
    requires requires(T&& t, scheduler& s) { t.set_scheduler(s); }
T set_scheduler(T&& t, scheduler& s) {
    t.set_scheduler(s);
    return std::move(t);
}


} // namespace asyncpp