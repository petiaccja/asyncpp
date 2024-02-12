#include <asyncpp/testing/interleaver.hpp>

#include <algorithm>
#include <numeric>
#include <ranges>
#include <regex>
#include <sstream>
#include <stack>
#include <stdexcept>


namespace asyncpp::testing {


constinit const thread_state thread_state::running = thread_state{ code::running };
constinit const thread_state thread_state::blocked = thread_state{ code::blocked };
constinit const thread_state thread_state::completed = thread_state{ code::completed };

static_assert(std::atomic<thread_state>::is_always_lock_free);

thread_local thread* thread::current_thread = nullptr;


namespace impl_suspension {
    void wait(suspension_point& sp) {
        thread::suspend_this_thread(sp);
    }
} // namespace impl_suspension


void thread::resume() {
    const auto current_state = m_content->state.load();
    const auto current_suspension_point = current_state.get_suspension_point();
    if (!current_suspension_point) {
        throw std::logic_error("thread must be suspended at a suspension point");
    }
    const auto next_state = current_suspension_point->acquire ? thread_state::blocked : thread_state::running;
    m_content->state.store(next_state);
}


void thread::suspend_this_thread(const suspension_point& sp) {
    if (current_thread) {
        current_thread->suspend(sp);
    }
}


thread_state thread::get_state() const {
    return m_content->state.load();
}


void thread::suspend(const suspension_point& sp) {
    m_content->state.store(thread_state::suspended(sp));
    while (m_content->state.load().is_suspended()) {
        // Wait.
    }
}


std::vector<thread_state> stabilize(std::span<std::unique_ptr<thread>> threads) {
    using namespace std::chrono_literals;

    std::vector<thread_state> states;
    const auto start = std::chrono::high_resolution_clock::now();
    do {
        const auto elapsed = std::chrono::high_resolution_clock::now() - start;
        if (elapsed > 200ms) {
            throw std::logic_error("deadlock");
        }
        states = get_states(threads);
    } while (!is_stable(states));

    while (!is_unblocked(states)) {
        const auto elapsed = std::chrono::high_resolution_clock::now() - start;
        if (elapsed > 200ms) {
            throw std::logic_error("deadlock");
        }
        states = get_states(threads);
    }

    return states;
}


std::vector<thread_state> get_states(std::span<std::unique_ptr<thread>> threads) {
    std::vector<thread_state> states;
    std::ranges::transform(threads, std::back_inserter(states), [](const auto& th) {
        return th->get_state();
    });
    return states;
}


bool is_stable(const std::vector<thread_state>& states) {
    return std::ranges::all_of(states, [](const auto& state) { return state.is_stable(); });
}


bool is_unblocked(const std::vector<thread_state>& states) {
    const auto num_blocked = std::ranges::count_if(states, [](const auto& state) { return state == thread_state::blocked; });
    const auto num_suspended = std::ranges::count_if(states, [](const auto& state) { return state.is_suspended(); });
    if (num_blocked != 0) {
        return num_suspended != 0;
    }
    return true;
}


int select_resumed(const swarm_state& state, const tree::transition_node& node) {
    std::vector<int> all(state.thread_states.size());
    std::iota(all.begin(), all.end(), 0);

    auto viable = all | std::views::filter([&](int thread_idx) { return state.thread_states[thread_idx].is_suspended(); });
    auto undiscovered = viable | std::views::filter([&](int thread_idx) { return !node.successors.contains(thread_idx); });
    auto incomplete = viable | std::views::filter([&](int thread_idx) { return !node.completed.contains(thread_idx); });

    if (!undiscovered.empty()) {
        return undiscovered.front();
    }
    if (!incomplete.empty()) {
        return incomplete.front();
    }
    if (!viable.empty()) {
        return viable.front();
    }
    throw std::logic_error("no thread can be resumed");
}


bool is_transitively_complete(const tree& tree, const tree::stable_node& node) {
    std::vector<bool> completed_paths;
    for (const auto& [swarm, transition] : node.swarm_states) {
        completed_paths.resize(swarm.thread_states.size(), false);
        for (size_t resumed = 0; resumed < swarm.thread_states.size(); ++resumed) {
            const auto& ts = swarm.thread_states[resumed];
            if (ts == thread_state::completed) {
                completed_paths[resumed] = true;
            }
        }
        for (const int resumed : transition->completed) {
            completed_paths[resumed] = true;
        }
    }
    return std::ranges::all_of(completed_paths, [](auto v) { return v; });
}


void mark_complete(tree& tree, tree::stable_node& node) {
    if (&node == &tree.root()) {
        return;
    }
    auto& transition = tree.previous(node);
    const auto resumed_it = std::ranges::find_if(transition.successors, [&](auto& s) { return s.second.get() == &node; });
    assert(resumed_it != transition.successors.end());
    const int resumed = resumed_it->first;
    transition.completed.insert(resumed);

    auto& prev_node = tree.previous(transition);
    if (is_transitively_complete(tree, prev_node)) {
        return mark_complete(tree, prev_node); // Stack overflow risk: tail recursion should optimize out to loop.
    }
}


void run_next_interleaving(tree& tree, std::span<std::unique_ptr<thread>> swarm) {
    auto current_node = &tree.root();

    do {
        const auto state = swarm_state(stabilize(swarm));
        if (std::ranges::all_of(state.thread_states, [](const auto& ts) { return ts == thread_state::completed; })) {
            break;
        }
        auto& transition_node = tree.next(*current_node, state);
        const auto resumed = select_resumed(state, transition_node);
        swarm[resumed]->resume();
        current_node = &tree.next(transition_node, resumed);
    } while (true);

    mark_complete(tree, *current_node);
}


void thread::initialize_this_thread() {
    current_thread = this;
}

} // namespace asyncpp::testing