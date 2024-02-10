#pragma once

#include <atomic>
#include <compare>
#include <limits>
#include <string>


namespace asyncpp::testing {

struct suspension_point;


class sequencer {
public:
    struct state {
        std::strong_ordering operator<=>(const state&) const noexcept = default;
        enum {
            waiting,
            running,
            acquiring,
        } activity;
        suspension_point* location = nullptr;
    };

    static inline suspension_point* const RUNNING = reinterpret_cast<suspension_point*>(std::numeric_limits<size_t>::max() - 2);
    static inline suspension_point* const ACQUIRING = reinterpret_cast<suspension_point*>(std::numeric_limits<size_t>::max() - 1);

    sequencer(std::string name = {});

    auto get_name() const -> std::string_view;
    auto get_state() const -> state;
    void wait(suspension_point& location);
    bool allow();

private:
    std::atomic<suspension_point*> m_location = RUNNING;
    std::string m_name;
};

} // namespace asyncpp::interleaving