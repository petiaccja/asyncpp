#include <asyncpp/interleaving/runner.hpp>
#include <asyncpp/interleaving/sequence_point.hpp>
#include <asyncpp/interleaving/sequencer.hpp>
#include <asyncpp/interleaving/state_tree.hpp>

#include <csignal>
#include <format>
#include <map>
#include <ranges>
#include <span>
#include <thread>


namespace asyncpp::interleaving {


std::ostream& operator<<(std::ostream& os, const sequence_point& st);
std::ostream& operator<<(std::ostream& os, const sequencer::state& st);

namespace impl_sp {


    struct timeout_error : std::runtime_error {
        timeout_error() : std::runtime_error("timeout") {}
    };


    sequence_point initial_point{ .acquire = false, .name = "<start>", .file = __FILE__, .line = __LINE__ };
    sequence_point final_point{ .acquire = false, .name = "<finish>", .file = __FILE__, .line = __LINE__ };
    thread_local std::shared_ptr<sequencer> local_sequencer;
    thread_local filter local_filter;


    interleaving* g_current_interleaving = nullptr;


    void signal_handler(int sig) {
        if (g_current_interleaving) {
            std::cout << interleaving_printer{ *g_current_interleaving, true } << std::endl;
            g_current_interleaving = nullptr;
        }
        std::quick_exit(-1);
    }


    template <class Func>
    void wait_while(Func func, std::chrono::milliseconds timeout) {
        const auto start = std::chrono::high_resolution_clock::now();
        while (func()) {
            const auto now = std::chrono::high_resolution_clock::now();
            if (now - start > timeout) {
                throw timeout_error();
            }
        }
    }


    void wait(sequence_point& sp) {
        if (local_sequencer) {
            if (local_filter(sp)) {
                local_sequencer->wait(sp);
            }
        }
    }


    static state get_current_states(std::span<const std::shared_ptr<sequencer>> sequencers) {
        state state_;
        for (const auto& seq : sequencers) {
            const auto seq_state = seq->get_state();
            state_.m_sequencer_states[seq] = seq_state;
        }
        return state_;
    }


    static state sync_sequencers(std::span<const std::shared_ptr<sequencer>> sequencers, std::chrono::milliseconds timeout) {
        state state_;
        const auto verify_synced = [&state_, &sequencers] {
            state_ = get_current_states(sequencers);
            const auto synced = std::ranges::all_of(state_.m_sequencer_states, [](const auto& seq_state) {
                return seq_state.second.activity == sequencer::state::acquiring || seq_state.second.activity == sequencer::state::waiting;
            });
            return synced;
        };
        wait_while([&] { return !verify_synced(); }, timeout);
        return state_;
    }


    auto select_allowed_sequencer(const state& state, std::span<const std::shared_ptr<sequencer>> excluded = {}) -> std::shared_ptr<sequencer> {
        const auto is_waiting = [](const sequencer::state& seq_state) {
            return seq_state.activity == sequencer::state::waiting;
        };
        const auto is_finished = [](const sequencer::state& seq_state) {
            return seq_state.location == &final_point;
        };

        std::vector<std::shared_ptr<sequencer>> choices;
        for (const auto& [seq, seq_state] : state.m_sequencer_states) {
            if (is_waiting(seq_state) && !is_finished(seq_state) && std::ranges::find(excluded, seq) == excluded.end()) {
                choices.push_back(seq);
            }
        }
        std::ranges::sort(choices, [](const auto& lhs, const auto& rhs) { return lhs->get_name() < rhs->get_name(); });

        return choices.empty() ? nullptr : choices.front();
    }


    auto get_excluded_sequencers(const state_tree& tree) -> std::vector<std::shared_ptr<sequencer>> {
        const auto is_completed = [&tree](const auto& seq) {
            for (const auto& [key, branch] : tree.get_branches()) {
                if (key.first == seq && branch->is_marked_complete()) {
                    return true;
                }
            }
            return false;
        };

        std::vector<std::shared_ptr<sequencer>> excluded;
        for (const auto& [seq, seq_state] : tree.get_state().m_sequencer_states) {
            if (is_completed(seq)) {
                excluded.push_back(seq);
            }
        }
        return excluded;
    }


    auto is_complete(const state_tree& tree) -> bool {
        return select_allowed_sequencer(tree.get_state(), get_excluded_sequencers(tree)) == nullptr;
    }


    void task_thread_func(std::shared_ptr<sequencer> seq, std::function<void()> func, filter filter_) {
        local_filter = std::move(filter_);
        local_sequencer = seq;
        seq->wait(initial_point);
        func();
        seq->wait(final_point);
    }


    interleaving control_thread_func(std::span<const std::shared_ptr<sequencer>> sequencers, std::shared_ptr<state_tree> tree) {
        using namespace std::chrono_literals;
        sync_sequencers(sequencers, 200ms);

        std::vector path = { tree };
        interleaving interleaving;

        g_current_interleaving = &interleaving;
        const auto prev_handler = std::signal(SIGABRT, &signal_handler);

        std::shared_ptr<sequencer> selected;
        do {
            try {
                selected = nullptr;
                const auto excluded = get_excluded_sequencers(*path.back());
                state selection_state;
                const auto select = [&] {
                    selection_state = get_current_states(sequencers);
                    const bool final = std::ranges::all_of(selection_state.m_sequencer_states | std::views::values, [](auto& v) { return v.location == &final_point; });
                    if (final) {
                        return false;
                    }
                    selected = select_allowed_sequencer(selection_state, excluded);
                    return selected == nullptr;
                };
                try {
                    wait_while(select, 200ms);
                }
                catch (timeout_error&) {
                    throw deadlock_error();
                }
                if (selected) {
                    interleaving.emplace_back(selected->get_name(), selection_state.m_sequencer_states.at(selected));
                    selected->allow();
                    const auto synced_state = sync_sequencers(sequencers, 200ms);
                    const auto branch = path.back()->branch(selected, synced_state);
                    path.push_back(branch);
                }
            }
            catch (timeout_error&) {
                std::cerr << interleaving_printer{ interleaving, true };
                std::cerr << "\nTIMED OUT";
                std::terminate();
            }
            catch (deadlock_error&) {
                std::cerr << interleaving_printer{ interleaving, true };
                std::cerr << "\nDEADLOCK";
                std::terminate();
            }
        } while (selected);
        for (const auto& branch : std::views::reverse(path)) {
            if (is_complete(*branch)) {
                branch->mark_complete();
                branch->prune();
            }
        }

        sync_sequencers(sequencers, 200ms);
        for (const auto& exec_thread : sequencers) {
            exec_thread->allow();
        }

        std::signal(SIGABRT, prev_handler);
        g_current_interleaving = nullptr;

        return interleaving;
    }

} // namespace impl_sp


generator<interleaving> run_all(std::function<std::any()> fixture, std::vector<std::function<void(std::any&)>> threads, std::vector<std::string_view> names, filter filter_) {
    using namespace impl_sp;

    std::vector<std::shared_ptr<sequencer>> sequencers;
    for (size_t i = 0; i < threads.size(); ++i) {
        std::string name = i < names.size() ? std::string(names[i]) : std::format("${}", i);
        const auto seq = std::make_shared<sequencer>(std::move(name));
        sequencers.push_back(seq);
    }

    std::map<std::shared_ptr<sequencer>, sequencer::state> initial_state;
    for (const auto& exec_thread : sequencers) {
        initial_state[exec_thread] = { sequencer::state::waiting, &initial_point };
    }
    const auto root = std::make_shared<state_tree>(state{ initial_state });

    do {
        interleaving interleaving_;
        {
            std::vector<std::jthread> os_threads;
            auto fixture_val = fixture();
            for (size_t i = 0; i < threads.size(); ++i) {
                os_threads.emplace_back([exec_thread = sequencers[i], &tsk = threads[i], &fixture_val, &filter_] {
                    task_thread_func(
                        exec_thread, [&] { tsk(fixture_val); }, filter_);
                });
            }
            interleaving_ = control_thread_func(sequencers, root);
        }
        co_yield interleaving_;
    } while (!root->is_marked_complete());
}


std::ostream& operator<<(std::ostream& os, const sequence_point& st) {
    os << st.function << "\n";
    os << "    " << st.name;
    if (st.acquire) {
        os << " [ACQUIRE]";
    }
    os << "\n";
    if (!st.file.empty()) {
        os << "    " << st.file << ":" << st.line;
    }
    return os;
}


std::ostream& operator<<(std::ostream& os, const sequencer::state& st) {
    switch (st.activity) {
        case sequencer::state::waiting: return os << *st.location;
        case sequencer::state::running: return os << "running";
        case sequencer::state::acquiring: return os << "acquiring";
        default: std::terminate();
    }
}


std::ostream& operator<<(std::ostream& os, const interleaving_printer& il) {
    if (il.detail) {
        size_t idx = 1;
        for (auto& node : il.il) {
            os << idx++ << ". " << node.first << ": " << node.second << std::endl;
        }
    }
    else {
        for (auto& node : il.il) {
            os << "{" << node.first << ": ";
            switch (node.second.activity) {
                case sequencer::state::waiting: os << node.second.location->name; break;
                case sequencer::state::running: os << "running"; break;
                case sequencer::state::acquiring: os << "acquiring"; break;
                default: std::terminate();
            }
            os << "} -> ";
        }
        os << "X";
    }
    return os;
}


bool filter::operator()(const sequence_point& point) const {
    return std::regex_search(point.file.begin(), point.file.end(), m_files);
}


} // namespace asyncpp::interleaving