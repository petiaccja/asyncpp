#include <asyncpp/testing/interleaver.hpp>

#include <algorithm>
#include <format>
#include <numeric>
#include <ranges>
#include <regex>
#include <sstream>
#include <stack>
#include <stdexcept>


namespace asyncpp::testing {


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
    assert(current_suspension_point && "thread must be suspended at a suspension point");
    const auto next_state = current_suspension_point->acquire ? thread_state::blocked() : thread_state::running();
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
    const auto prev_state = m_content->state.exchange(thread_state::suspended(sp));
    assert(prev_state == thread_state::running() || prev_state == thread_state::blocked());
    while (m_content->state.load().is_suspended()) {
        // Wait.
    }
}


tree::stable_node& tree::root() {
    return *m_root;
}


tree::transition_node& tree::next(stable_node& node, const swarm_state& state) {
    const auto it = node.swarm_states.find(state);
    if (it != node.swarm_states.end()) {
        return *it->second;
    }
    const auto successor = std::make_shared<transition_node>(std::map<int, std::shared_ptr<stable_node>>{}, node.weak_from_this());
    return *node.swarm_states.insert_or_assign(state, successor).first->second;
}


tree::stable_node& tree::next(transition_node& node, int resumed) {
    const auto it = node.successors.find(resumed);
    if (it != node.successors.end()) {
        return *it->second;
    }
    const auto successor = std::make_shared<stable_node>(std::map<swarm_state, std::shared_ptr<transition_node>>{}, node.weak_from_this());
    return *node.successors.insert_or_assign(resumed, successor).first->second;
}


tree::transition_node& tree::previous(stable_node& node) {
    const auto ptr = node.predecessor.lock();
    assert(ptr);
    return *ptr;
}


tree::stable_node& tree::previous(transition_node& node) {
    const auto ptr = node.predecessor.lock();
    assert(ptr);
    return *ptr;
}


std::string dump(const swarm_state& state) {
    std::stringstream ss;
    for (const auto& th : state.thread_states) {
        if (th == thread_state::completed()) {
            ss << "completed";
        }
        else if (th == thread_state::blocked()) {
            ss << "blocked";
        }
        else if (th == thread_state::running()) {
            ss << "running";
        }
        else {
            ss << th.get_suspension_point()->function << "::" << th.get_suspension_point()->name;
        }
        ss << "\n";
    }
    return ss.str();
}


std::string tree::dump() const {
    std::stringstream ss;

    std::stack<const stable_node*> worklist;
    worklist.push(m_root.get());

    ss << "digraph G {\n";

    while (!worklist.empty()) {
        const auto node = worklist.top();
        worklist.pop();

        size_t state_idx = 0;
        for (const auto& [swarm, transition] : node->swarm_states) {
            const auto node_name = std::format("_{}_{}", (void*)node, state_idx++);
            const auto transition_name = std::format("_{}", (void*)transition.get());

            auto state = asyncpp::testing::dump(swarm);
            state = std::regex_replace(state, std::regex("\n"), "\\n");
            state = std::regex_replace(state, std::regex("\""), "\\\"");
            ss << std::format("{} [label=\"{}\"];\n", node_name, state);
            ss << node_name << " -> " << transition_name << ";\n";

            std::stringstream complete;
            for (const auto v : transition->completed) {
                complete << v << " ";
            }
            ss << std::format("{} [label=\"complete: [{}]\"];\n", transition_name, complete.str());

            if (const auto predecessor = node->predecessor.lock()) {
                const auto resumed = std::ranges::find_if(predecessor->successors, [&](const auto& s) { return s.second.get() == node; })->first;
                const auto predecessor_name = std::format("_{}", (void*)predecessor.get());
                ss << predecessor_name << " -> " << node_name << " [label=" << resumed << "];\n";
            }

            for (const auto& [resumed, next] : transition->successors) {
                worklist.emplace(next.get());
            }
        }
    }

    ss << "}";

    return ss.str();
}


std::vector<thread_state> stabilize(std::span<std::unique_ptr<thread>> threads) {
    using namespace std::chrono_literals;

    std::vector<thread_state> states;
    const auto start = std::chrono::high_resolution_clock::now();
    do {
        const auto elapsed = std::chrono::high_resolution_clock::now() - start;
        if (elapsed > 200ms) {
            throw std::logic_error("deadlock - still running");
        }
        states = get_states(threads);
    } while (!is_stable(states));

    while (!is_unblocked(states)) {
        const auto elapsed = std::chrono::high_resolution_clock::now() - start;
        if (elapsed > 200ms) {
            throw std::logic_error("deadlock - all blocked");
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
    const auto num_blocked = std::ranges::count_if(states, [](const auto& state) { return state == thread_state::blocked(); });
    const auto num_suspended = std::ranges::count_if(states, [](const auto& state) { return state.is_suspended(); });
    if (num_blocked != 0) {
        return num_suspended != 0;
    }
    return true;
}


int select_resumed(const swarm_state& state, const tree::transition_node& node, const std::vector<std::map<const suspension_point*, size_t>>* hit_counts) {
    std::vector<int> all(state.thread_states.size());
    std::iota(all.begin(), all.end(), 0);

    auto viable = all | std::views::filter([&](int thread_idx) { return state.thread_states[thread_idx].is_suspended(); });
    auto undiscovered = viable | std::views::filter([&](int thread_idx) { return !node.successors.contains(thread_idx); });
    auto incomplete = viable | std::views::filter([&](int thread_idx) { return !node.completed.contains(thread_idx); });
    auto unrepeated = incomplete | std::views::filter([&](int thread_idx) {
                          if (!hit_counts) {
                              return true;
                          }
                          return !(*hit_counts)[thread_idx].contains(state.thread_states[thread_idx].get_suspension_point())
                                 || (*hit_counts)[thread_idx].at(state.thread_states[thread_idx].get_suspension_point()) < 3;
                      });

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
            if (ts == thread_state::completed() || ts == thread_state::blocked()) {
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


path run_next_interleaving(tree& tree, std::span<std::unique_ptr<thread>> swarm) {
    std::vector<std::map<const suspension_point*, size_t>> hit_counts(swarm.size());
    path path;
    auto current_node = &tree.root();

    do {
        try {
            const auto state = swarm_state(stabilize(swarm));
            path.steps.push_back({ state, -1 });
            if (std::ranges::all_of(state.thread_states, [](const auto& ts) { return ts == thread_state::completed(); })) {
                break;
            }
            for (size_t thread_idx = 0; thread_idx < swarm.size(); ++thread_idx) {
                const auto& ts = state.thread_states[thread_idx];
                if (const auto sp = ts.get_suspension_point(); sp != nullptr) {
                    auto it = hit_counts[thread_idx].find(sp);
                    if (it == hit_counts[thread_idx].end()) {
                        it = hit_counts[thread_idx].insert_or_assign(sp, 0).first;
                    }
                    ++it->second;
                }
            }
            auto& transition_node = tree.next(*current_node, state);
            const auto resumed = select_resumed(state, transition_node, &hit_counts);
            path.steps.back().second = resumed;
            swarm[resumed]->resume();
            current_node = &tree.next(transition_node, resumed);
        }
        catch (std::exception&) {
            const auto locked_states = get_states(swarm);
            path.steps.push_back({ swarm_state(locked_states), -1 });
            std::cerr << path.dump() << std::endl;
            std::terminate();
        }
    } while (true);

    mark_complete(tree, *current_node);
    return path;
}


std::string path::dump() const {
    std::stringstream ss;
    for (const auto& step : steps) {
        ss << ::asyncpp::testing::dump(step.first) << std::endl;
        ss << "  -> " << step.second << std::endl;
    }
    return ss.str();
}


void thread::initialize_this_thread() {
    current_thread = this;
}

} // namespace asyncpp::testing