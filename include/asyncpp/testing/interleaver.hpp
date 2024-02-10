#pragma once

#include "suspension_point.hpp"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <iostream>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <thread>
#include <vector>


namespace asyncpp::testing {


struct thread_state {
private:
    enum class code : size_t {
        running = ~size_t(0) - 1,
        blocked = ~size_t(0) - 2,
        completed = ~size_t(0) - 3,
    };

public:
    auto operator<=>(const thread_state& rhs) const = default;

    const suspension_point* get_suspension_point() const {
        return is_suspended() ? reinterpret_cast<const suspension_point*>(value) : nullptr;
    }

    bool is_suspended() const {
        return value != code::running && value != code::blocked && value != code::completed;
    }

    bool is_stable() const {
        return value != code::running;
    }

    static thread_state suspended(const suspension_point& sp) {
        return thread_state{ static_cast<code>(reinterpret_cast<size_t>(&sp)) };
    }

    static constinit const thread_state running;
    static constinit const thread_state blocked;
    static constinit const thread_state completed;

    code value = code::running;
};


class thread {
public:
    thread() = default;
    thread(thread&&) = delete;

    template <class Func, class... Args>
    thread(Func func, Args&&... args) {
        const auto wrapper = [ this, func ]<Args... args>(Args && ... args) {
            initialize_this_thread();
            INTERLEAVED("initial_point");
            func(std::forward<Args>(args)...);
            m_content->state = thread_state::completed;
        };
        m_content->thread = std::jthread(wrapper, std::forward<Args>(args)...);
    }

    void resume();
    static void suspend_this_thread(const suspension_point& sp);
    thread_state get_state() const;

private:
    void initialize_this_thread();
    void suspend(const suspension_point& sp);

private:
    struct content {
        std::jthread thread;
        std::atomic<thread_state> state = thread_state::running;
    };
    std::unique_ptr<content> m_content = std::make_unique<content>();
    static thread_local thread* current_thread;
};


template <class Scenario>
struct thread_function {
    std::string name;
    void (*func)(Scenario&);
};


class interleaving_graph {
public:
    struct vertex {
        std::vector<thread_state> state;
        auto operator<=>(const vertex&) const = default;
    };

    struct neighbor_list {
        std::vector<vertex> in_edges;
        std::vector<vertex> out_edges;
    };

public:
    bool contains(const vertex& v) {
        return m_vertices.contains(v);
    }

    std::span<const vertex> in_edges(const vertex& v) const {
        const auto it = m_vertices.find(v);
        assert(it != m_vertices.end());
        return it->second.in_edges;
    }

    std::span<const vertex> out_edges(const vertex& v) const {
        const auto it = m_vertices.find(v);
        assert(it != m_vertices.end());
        return it->second.out_edges;
    }

    void add_vertex(vertex v) {
        m_vertices.insert_or_assign(std::move(v), neighbor_list{});
    }

    void add_edge(const vertex& source, const vertex& target) {
        const auto source_it = m_vertices.find(source);
        const auto target_it = m_vertices.find(target);
        assert(source_it != m_vertices.end());
        source_it->second.out_edges.push_back(target);
        target_it->second.in_edges.push_back(source);
    }

    std::string dump(std::function<bool(const vertex&)> completion = {}, std::function<size_t(const vertex&, const vertex&)> transition = {}) const;

private:
    std::map<vertex, neighbor_list> m_vertices;
};


struct interleaving_graph_state {
    interleaving_graph graph;
    std::map<interleaving_graph::vertex, bool> completion_map;
    std::map<std::pair<interleaving_graph::vertex, interleaving_graph::vertex>, size_t> transition_map;

    std::string dump() const;
};


template <class Scenario>
std::vector<std::unique_ptr<thread>> launch_threads(const std::vector<thread_function<Scenario>>& thread_funcs) {
    const auto scenario = std::make_shared<Scenario>();
    std::vector<std::unique_ptr<thread>> threads;
    for (const auto& thread_func : thread_funcs) {
        threads.push_back(std::make_unique<thread>([scenario, func = thread_func.func] { func(*scenario); }));
    }
    return threads;
}


interleaving_graph::vertex stabilize(std::span<std::unique_ptr<thread>> threads);
bool interleave(interleaving_graph_state& graph, std::span<std::unique_ptr<thread>> threads);
std::vector<thread_state> get_states(std::span<std::unique_ptr<thread>> threads);
size_t select_resumed(interleaving_graph_state& graph, const interleaving_graph::vertex& v);
bool is_stable(const interleaving_graph::vertex& v);
bool is_completed(const interleaving_graph::vertex& v);
void mark_completed(interleaving_graph_state& graph, const interleaving_graph::vertex& v);


template <class Scenario>
class interleaver {
public:
    explicit interleaver(std::vector<thread_function<Scenario>> thread_funcs)
        : m_thread_funcs(std::move(thread_funcs)) {}

    void run() {
        interleaving_graph_state graph;
        bool completed;
        do {
            auto threads = launch_threads(m_thread_funcs);
            completed = interleave(graph, threads);
        } while (!completed);
        std::cout << graph.dump() << std::endl;
    }

private:
    std::vector<thread_function<Scenario>> m_thread_funcs;
};


} // namespace asyncpp::testing


#define INTERLEAVED_TEST(SCENARIO, ...) \
    asyncpp::testing::interleaver<SCENARIO>({ __VA_ARGS__ }).run()

#define INTERLEAVED_THREAD(NAME, METHOD) \
    asyncpp::testing::thread_function(NAME, METHOD)