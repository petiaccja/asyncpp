#include <asyncpp/testing/state_tree.hpp>


namespace asyncpp::testing {


state_tree::state_tree(state state)
    : m_state(std::move(state)) {}


std::shared_ptr<state_tree> state_tree::branch(const std::shared_ptr<sequencer>& allowed_thread,
                                               const state& resulting_state) {
    branch_key key(allowed_thread, resulting_state);
    auto it = m_branches.find(key);
    if (it == m_branches.end()) {
        const auto br = std::make_shared<state_tree>(resulting_state);
        it = m_branches.insert_or_assign(key, br).first;
    }
    return it->second;
}


const state& state_tree::get_state() const {
    return m_state;
}


auto state_tree::get_branches() const -> const std::map<branch_key, std::shared_ptr<state_tree>>& {
    return m_branches;
}


void state_tree::mark_complete() {
    m_complete = true;
}


bool state_tree::is_marked_complete() const {
    return m_complete;
}


void state_tree::prune() {
    m_branches = {};
}


} // namespace asyncpp::interleaving