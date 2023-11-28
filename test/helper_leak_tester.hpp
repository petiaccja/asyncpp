#pragma once

#include <memory>


// Should be passed by value to a coroutine.
// Gets copied onto the coroutine stack and should get destructed
// when the coroutine promise is destroyed.
struct leak_tester {
    leak_tester() : m_count(std::make_shared<int>(0)) {}
    operator bool() { return m_count.use_count() == 1; }

private:
    std::shared_ptr<int> m_count;
};