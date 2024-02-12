#pragma once

#include "suspension_point.hpp"

#include <asyncpp/generator.hpp>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
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
        const auto wrapper = [this, func]<Args... args>(Args&&... args_) {
            initialize_this_thread();
            INTERLEAVED("initial_point");
            func(std::forward<Args>(args_)...);
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


template <std::convertible_to<std::string> Str, class Scenario>
thread_function(const Str&, void (*)(Scenario&)) -> thread_function<Scenario>;


struct swarm_state {
    std::vector<thread_state> thread_states;

    auto operator<=>(const swarm_state&) const = default;
};


class tree {
public:
    struct transition_node;

    struct stable_node : std::enable_shared_from_this<stable_node> {
        stable_node() = default;
        stable_node(std::map<swarm_state, std::shared_ptr<transition_node>> swarm_states,
                    std::weak_ptr<transition_node> predecessor)
            : swarm_states(std::move(swarm_states)), predecessor(std::move(predecessor)) {}
        std::map<swarm_state, std::shared_ptr<transition_node>> swarm_states;
        std::weak_ptr<transition_node> predecessor;
    };

    struct transition_node : std::enable_shared_from_this<transition_node> {
        transition_node() = default;
        transition_node(std::map<int, std::shared_ptr<stable_node>> successors,
                        std::weak_ptr<stable_node> predecessor)
            : successors(std::move(successors)), predecessor(std::move(predecessor)) {}
        std::map<int, std::shared_ptr<stable_node>> successors;
        std::set<int> completed;
        std::weak_ptr<stable_node> predecessor;
    };

public:
    stable_node& root() {
        return *m_root;
    }

    transition_node& next(stable_node& node, const swarm_state& state) {
        const auto it = node.swarm_states.find(state);
        if (it != node.swarm_states.end()) {
            return *it->second;
        }
        const auto successor = std::make_shared<transition_node>(std::map<int, std::shared_ptr<stable_node>>{}, node.weak_from_this());
        return *node.swarm_states.insert_or_assign(state, successor).first->second;
    }

    stable_node& next(transition_node& node, int resumed) {
        const auto it = node.successors.find(resumed);
        if (it != node.successors.end()) {
            return *it->second;
        }
        const auto successor = std::make_shared<stable_node>(std::map<swarm_state, std::shared_ptr<transition_node>>{}, node.weak_from_this());
        return *node.successors.insert_or_assign(resumed, successor).first->second;
    }

    transition_node& previous(stable_node& node) {
        const auto ptr = node.predecessor.lock();
        assert(ptr);
        return *ptr;
    }

    stable_node& previous(transition_node& node) {
        const auto ptr = node.predecessor.lock();
        assert(ptr);
        return *ptr;
    }

private:
    std::shared_ptr<stable_node> m_root = std::make_shared<stable_node>();
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

std::vector<thread_state> stabilize(std::span<std::unique_ptr<thread>> threads);
std::vector<thread_state> get_states(std::span<std::unique_ptr<thread>> threads);
bool is_stable(const std::vector<thread_state>& v);
bool is_unblocked(const std::vector<thread_state>& states);
int select_resumed(const swarm_state& state, const tree::transition_node& node);
bool is_transitively_complete(const tree& tree, const tree::stable_node& node);
void mark_complete(tree& tree, tree::stable_node& node);
void run_next_interleaving(tree& tree, std::span<std::unique_ptr<thread>> swarm);


template <class Scenario>
class interleaver {
public:
    explicit interleaver(std::vector<thread_function<Scenario>> thread_funcs)
        : m_thread_funcs(std::move(thread_funcs)) {}

    void run() {
        tree tree;
        int max = 100;
        do {
            auto swarm = launch_threads(m_thread_funcs);
            run_next_interleaving(tree, swarm);
        } while (!is_transitively_complete(tree, tree.root()) && max-- > 0);
    }

private:
    std::vector<thread_function<Scenario>> m_thread_funcs;
};


} // namespace asyncpp::testing


#define INTERLEAVED_TEST(SCENARIO, ...) \
    asyncpp::testing::interleaver<SCENARIO>({ __VA_ARGS__ }).run()

#define INTERLEAVED_THREAD(NAME, METHOD) \
    asyncpp::testing::thread_function(NAME, METHOD)