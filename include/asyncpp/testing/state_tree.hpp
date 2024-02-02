#pragma once

#include "sequencer.hpp"

#include <compare>
#include <map>
#include <memory>


namespace asyncpp::testing {


struct state {
    std::strong_ordering operator<=>(const state&) const noexcept = default;
    std::map<std::shared_ptr<sequencer>, sequencer::state> m_sequencer_states;
};


class state_tree {
    using branch_key = std::pair<std::shared_ptr<sequencer>, state>;

public:
    state_tree(state state);

    auto branch(const std::shared_ptr<sequencer>& allowed_thread, const state& resulting_state) -> std::shared_ptr<state_tree>;
    auto get_state() const -> const state&;
    auto get_branches() const -> const std::map<branch_key, std::shared_ptr<state_tree>>&;
    void mark_complete();
    bool is_marked_complete() const;
    void prune();

private:
    std::map<branch_key, std::shared_ptr<state_tree>> m_branches;
    state m_state;
    bool m_complete = false;
};

} // namespace asyncpp::interleaving