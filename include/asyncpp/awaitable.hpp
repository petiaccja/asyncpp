#pragma once

#include <exception>
#include <optional>
#include <variant>


namespace asyncpp {

template <class T>
struct basic_awaitable {
    virtual ~basic_awaitable() = default;
    virtual void on_ready() noexcept = 0;
};

} // namespace asyncpp