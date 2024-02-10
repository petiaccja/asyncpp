#include <asyncpp/testing/interleaver.hpp>

#include <numeric>
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


std::string interleaving_graph::dump(std::function<bool(const vertex&)> completion, std::function<size_t(const vertex&, const vertex&)> transition) const {
    static constexpr auto render_state = [](const thread_state& state) -> std::string {
        if (state == thread_state::running) {
            return "running";
        }
        if (state == thread_state::blocked) {
            return "blocked";
        }
        if (state == thread_state::completed) {
            return "completed";
        }
        const auto sp = state.get_suspension_point();
        if (sp != nullptr) {
            std::stringstream ss;
            ss << sp->function << "::" << sp->name;
            return std::regex_replace(ss.str(), std::regex("\""), "\\\"");
        }
        return "unknown";
    };

    std::stringstream ss;
    ss << "digraph G {\n";
    for (const auto& v : m_vertices) {
        ss << "n" << &v << " [label=\"";
        for (const auto& state : v.first.state) {
            ss << render_state(state) << "\\n";
        }
        if (completion) {
            ss << (completion(v.first) ? "<< completed >>" : "");
        }
        ss << "\"];\n";
    }
    for (const auto& v : m_vertices) {
        for (const auto& successor : out_edges(v.first)) {
            const auto& it = m_vertices.find(successor);
            ss << "n" << &v << " -> "
               << "n" << &it->first;
            if (transition) {
                ss << " [label=\"" << transition(v.first, it->first) << "\"]";
            }
            ss << ";\n";
        }
    }
    ss << "}";
    return ss.str();
}


std::string interleaving_graph_state::dump() const {
    const auto completion_func = [this](const interleaving_graph::vertex& v) {
        return completion_map.at(v);
    };
    const auto transition_func = [this](const interleaving_graph::vertex& src, const interleaving_graph::vertex& tar) {
        return transition_map.at({ src, tar });
    };
    return graph.dump(completion_func, transition_func);
}


interleaving_graph::vertex stabilize(std::span<std::unique_ptr<thread>> threads) {
    interleaving_graph::vertex vertex;
    do {
        vertex = interleaving_graph::vertex(get_states(threads));
    } while (!is_stable(vertex));
    return vertex;
}


bool interleave(interleaving_graph_state& graph, std::span<std::unique_ptr<thread>> threads) {
    const auto initial_vertex = stabilize(threads);
    interleaving_graph::vertex current_vertex = initial_vertex;

    if (!graph.graph.contains(current_vertex)) {
        graph.graph.add_vertex(current_vertex);
        graph.completion_map[current_vertex] = false;
    }

    while (!is_completed(current_vertex)) {
        // Select which thread to resume.
        const size_t resumed = select_resumed(graph, current_vertex);
        threads[resumed]->resume();

        // Wait until all threads are stable.
        interleaving_graph::vertex next_vertex = stabilize(threads);

        // Add next vertex to state graph if not already present.
        if (!graph.graph.contains(next_vertex)) {
            graph.graph.add_vertex(next_vertex);
            graph.completion_map[next_vertex] = false;
        }

        // Connect current vertex to next vertex.
        if (!graph.transition_map.contains({ current_vertex, next_vertex })) {
            graph.graph.add_edge(current_vertex, next_vertex);
            graph.transition_map[{ current_vertex, next_vertex }] = resumed;
        }

        current_vertex = std::move(next_vertex);
    }

    mark_completed(graph, current_vertex);

    return graph.completion_map[initial_vertex];
}


std::vector<thread_state> get_states(std::span<std::unique_ptr<thread>> threads) {
    std::vector<thread_state> states;
    std::ranges::transform(threads, std::back_inserter(states), [](const auto& th) {
        return th->get_state();
    });
    return states;
}


size_t select_resumed(interleaving_graph_state& graph, const interleaving_graph::vertex& v) {
    std::vector suitable(v.state.size(), false);
    for (size_t i = 0; i < v.state.size(); ++i) {
        suitable[i] = v.state[i].is_suspended();
    }

    std::vector preferred = suitable;
    const auto& next_vertices = graph.graph.out_edges(v);
    for (const auto& next_vertex : next_vertices) {
        if (graph.completion_map[next_vertex]) {
            const auto resumed = graph.transition_map[{ v, next_vertex }];
            preferred[resumed] = false;
        }
    }

    for (size_t i = 0; i < v.state.size(); ++i) {
        if (preferred[i]) {
            return i;
        }
    }
    for (size_t i = 0; i < v.state.size(); ++i) {
        if (suitable[i]) {
            return i;
        }
    }

    const auto str = graph.dump();
    std::cout << str << std::endl;
    throw std::invalid_argument("all states are completed");
}


bool is_stable(const interleaving_graph::vertex& v) {
    return std::ranges::all_of(v.state, [](const auto& state) { return state.is_stable(); });
}


bool is_completed(const interleaving_graph::vertex& v) {
    return std::ranges::all_of(v.state, [](const auto& state) { return state == thread_state::completed; });
}


void mark_completed(interleaving_graph_state& graph, const interleaving_graph::vertex& v) {
    std::stack<interleaving_graph::vertex> work_items;
    work_items.push(v);

    while (!work_items.empty()) {
        const auto item = std::move(work_items.top());
        work_items.pop();
        const auto& successors = graph.graph.out_edges(item);
        const size_t num_total = item.state.size() - std::ranges::count(item.state, thread_state::completed);
        const size_t num_complete = std::ranges::count_if(successors, [&graph](const auto& successor) {
            return graph.completion_map[successor];
        });
        if (num_total == num_complete || item == v) {
            graph.completion_map[item] = true;
            for (auto& ancestor : graph.graph.in_edges(item)) {
                //if (graph.completion_map[ancestor] == false) {
                    work_items.push(ancestor);
                //}
            }
        }
    }
}


void thread::initialize_this_thread() {
    current_thread = this;
}

} // namespace asyncpp::testing