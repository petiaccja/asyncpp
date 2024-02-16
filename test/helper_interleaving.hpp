#pragma once

#include "helper_schedulers.hpp"

#include <asyncpp/testing/suspension_point.hpp>

#include <concepts>

#include <catch2/catch_test_macros.hpp>


using namespace asyncpp;


template <template <class T> class Task>
struct await_task_scenario {
    thread_locked_scheduler awaiter_sched;
    thread_locked_scheduler awaited_sched;
    Task<int> result;

    await_task_scenario() {
        constexpr auto awaited = []() -> Task<int> {
            co_return 1;
        };
        constexpr auto awaiter = [](Task<int> awaited) -> Task<int> {
            co_return co_await awaited;
        };

        auto tmp = launch(awaited(), awaited_sched);
        result = launch(awaiter(std::move(tmp)), awaiter_sched);
    }

    void awaiter() {
        awaiter_sched.resume();
        if (!result.ready()) {
            INTERLEAVED_ACQUIRE(awaiter_sched.wait());
            awaiter_sched.resume();
        }
        REQUIRE(1 == join(result));
        result = {};
    }

    void awaited() {
        awaited_sched.resume();
    }
};


template <template <class T> class Task>
struct abandon_task_scenario {
    thread_locked_scheduler sched;
    Task<int> result;

    abandon_task_scenario() {
        constexpr auto func = []() -> Task<int> {
            co_return 1;
        };

        result = launch(func(), sched);
    }

    void task() {
        sched.resume();
        result = {};
    }

    void abandon() {
        result = {};
    }
};