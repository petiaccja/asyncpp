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
        event<int> evt;

        scenario() {
            const auto func = [this]() -> task<int> {
                co_return co_await evt;
            };

            launch(func(), sched);
        }

        void wait() {
            sched.resume();
        }

        void set() {
            evt.set_value(0);
        }
    };

    INTERLEAVED_RUN(scenario, THREAD("wait", &scenario::wait), THREAD("set", &scenario::set));
}
