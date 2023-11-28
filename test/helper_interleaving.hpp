#pragma once

#include "helper_leak_tester.hpp"
#include "helper_schedulers.hpp"

#include <async++/interleaving/runner.hpp>
#include <async++/interleaving/sequence_point.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace asyncpp;


template <class MainTask, class SubTask, class... MainArgs, class... SubArgs>
auto run_dependent_tasks(MainTask main_task,
                         SubTask sub_task,
                         std::tuple<MainArgs...> main_args,
                         std::tuple<SubArgs...> sub_args) {
    using sub_result_t = std::invoke_result_t<SubTask, SubArgs...>;
    using main_result_t = std::invoke_result_t<MainTask, sub_result_t, MainArgs...>;
    struct fixture {
        thread_locked_scheduler main_sched;
        thread_locked_scheduler sub_sched;
        main_result_t main_result;
    };

    const auto create_fixture = [&] {
        auto fixture_ = std::make_shared<fixture>();
        auto sub_result = launch(std::apply(sub_task, sub_args), fixture_->sub_sched);
        auto all_main_args = std::tuple_cat(std::forward_as_tuple(std::move(sub_result)), main_args);
        fixture_->main_result = launch(std::apply(main_task, std::move(all_main_args)), fixture_->main_sched);
        return fixture_;
    };
    const auto sub_thread = [](std::shared_ptr<fixture> fixture_) {
        fixture_->sub_sched.resume();
    };
    const auto main_thread = [](std::shared_ptr<fixture> fixture_) {
        fixture_->main_sched.resume();
        if (!fixture_->main_result.ready()) {
            INTERLEAVED_ACQUIRE(fixture_->main_sched.wait());
            fixture_->main_sched.resume();
        }
        join(fixture_->main_result);
    };

    return interleaving::run_all(std::function(create_fixture),
                                 std::vector{ std::function(main_thread), std::function(sub_thread) },
                                 { "$main", "$sub" });
}


template <class MainTask, class... MainArgs>
auto run_abandoned_task(MainTask main_task,
                        std::tuple<MainArgs...> main_args) {
    using main_result_t = std::invoke_result_t<MainTask, MainArgs...>;
    struct fixture {
        thread_locked_scheduler main_sched;
        main_result_t main_result;
    };

    const auto create_fixture = [&] {
        auto fixture_ = std::make_shared<fixture>();
        fixture_->main_result = launch(std::apply(main_task, std::move(main_args)), fixture_->main_sched);
        return fixture_;
    };
    const auto exec_thread = [](std::shared_ptr<fixture> fixture_) {
        fixture_->main_sched.resume();
    };
    const auto abandon_thread = [](std::shared_ptr<fixture> fixture_) {
        fixture_->main_result = {};
    };

    return interleaving::run_all(std::function(create_fixture),
                                 std::vector{ std::function(abandon_thread), std::function(exec_thread) },
                                 { "$abandon", "$exec" });
}


template <class InterleavingGen>
void evaluate_interleavings(InterleavingGen interleaving_gen, std::optional<leak_tester> tester = {}) {
    size_t count = 0;
    for (const auto& interleaving : interleaving_gen) {
        ++count;
        INFO((interleaving::interleaving_printer{ interleaving, true }));
        if (tester) {
            REQUIRE(tester.value());
        }
    }
    REQUIRE(count >= 3);
}