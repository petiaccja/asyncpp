#include "helper_interleaving.hpp"

#include <asyncpp/event.hpp>
#include <asyncpp/task.hpp>
#include <asyncpp/testing/interleaver.hpp>

#include <functional>

#include <catch2/catch_test_macros.hpp>

using namespace asyncpp;


TEST_CASE("Event: interleave co_await | set", "[Event]") {
    struct scenario {
        thread_locked_scheduler sched;
        task<int> result;
        event<int> evt;

        scenario() {
            const auto func = [this]() -> task<int> {
                co_return co_await evt;
            };

            result = launch(func(), sched);
        }
        scenario(scenario&&) = delete;

        void wait() {
            sched.resume();
            if (!result.ready()) {
                INTERLEAVED_ACQUIRE(sched.wait());
                sched.resume();
            }
            result = {};
        }

        void set() {
            evt.set_value(0);
        }
    };

    INTERLEAVED_RUN(scenario, THREAD("wait", &scenario::wait), THREAD("set", &scenario::set));
}
