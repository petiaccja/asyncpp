#include <asyncpp/testing/sequence_point.hpp>
#include <asyncpp/testing/sequencer.hpp>

#include <cassert>


namespace asyncpp::testing {

sequencer::sequencer(std::string name) : m_name(name) {}

void sequencer::wait(sequence_point& location) {
    m_location.store(&location);
    do {
    } while (m_location.load() == &location);
}


bool sequencer::allow() {
    const auto current_state = m_location.load();
    assert(current_state != ACQUIRING && current_state != RUNNING);
    const auto next_state = current_state->acquire ? ACQUIRING : RUNNING;
    const auto prev_state = m_location.exchange(next_state);
    return prev_state != ACQUIRING && prev_state != RUNNING;
}


auto sequencer::get_state() const -> state {
    const auto current_state = m_location.load();
    if (current_state == RUNNING) {
        return { .activity = state::running };
    }
    if (current_state == ACQUIRING) {
        return { .activity = state::acquiring };
    }
    return { .activity = state::waiting, .location = current_state };
}


std::string_view sequencer::get_name() const {
    return m_name;
}

} // namespace asyncpp::interleaving