#include <asyncpp/testing/interleaver.hpp>

#include <ranges>

#include <catch2/catch_test_macros.hpp>


using namespace asyncpp::testing;


static suspension_point p1;
static suspension_point p2;
static suspension_point p3;


// Number of combinations.
template <std::integral T>
T ncr(T n, T k) {
    T result = 1;
    for (auto factor = n; factor >= n - k + 1; --factor) {
        result *= factor;
    }
    for (auto divisor = k; divisor >= 1; --divisor) {
        result /= divisor;
    }
    return result;
}


TEST_CASE("Helper - nCr helper code", "[Helper]") {
    REQUIRE(ncr(1, 1) == 1);
    REQUIRE(ncr(4, 1) == 4);
    REQUIRE(ncr(4, 2) == 6);
}


TEST_CASE("Interleaver - next from stable node", "[Interleaver]") {
    const std::vector initial = { thread_state::suspended(p1) };

    SECTION("first time: add new state") {
        tree t;
        auto& transition = t.next(t.root(), swarm_state(initial));

        REQUIRE(t.root().swarm_states.size() == 1);
        REQUIRE(t.root().swarm_states.begin()->first == swarm_state(initial));
        REQUIRE(&t.previous(transition) == &t.root());
    }
    SECTION("subsequent times: fetch state") {
        tree t;
        auto& transition_1 = t.next(t.root(), swarm_state(initial));
        auto& transition_2 = t.next(t.root(), swarm_state(initial));
        REQUIRE(t.root().swarm_states.size() == 1);
        REQUIRE(&transition_1 == &transition_2);
    }
}


TEST_CASE("Interleaver - next from transition node", "[Interleaver]") {
    const std::vector initial = { thread_state::suspended(p1) };

    SECTION("first time: add new transition") {
        tree t;
        auto& transition = t.next(t.root(), swarm_state(initial));
        auto& next = t.next(transition, 0);

        REQUIRE(next.swarm_states.empty());
        REQUIRE(&t.previous(next) == &transition);
        REQUIRE(transition.completed.empty());
        REQUIRE(transition.successors.contains(0));
        REQUIRE(transition.successors.find(0)->second.get() == &next);
    }
    SECTION("subsequent times: fetch transition") {
        tree t;
        auto& transition = t.next(t.root(), swarm_state(initial));
        auto& next_1 = t.next(transition, 0);
        auto& next_2 = t.next(transition, 0);

        REQUIRE(transition.successors.contains(0));
        REQUIRE(transition.successors.find(0)->second.get() == &next_1);
        REQUIRE(transition.successors.find(0)->second.get() == &next_2);
    }
}


TEST_CASE("Interleaver - is transitively complete", "[Interleaver]") {
    const std::vector initial = { thread_state::suspended(p1) };
    const std::vector final = { thread_state::completed() };
    const std::vector blocked = { thread_state::blocked() };

    SECTION("empty root node") {
        tree t;
        REQUIRE(is_transitively_complete(t, t.root()));
    }
    SECTION("complete node") {
        tree t;
        t.next(t.root(), swarm_state(final));
        REQUIRE(is_transitively_complete(t, t.root()));
    }
    SECTION("complete transition") {
        tree t;
        auto& transition = t.next(t.root(), swarm_state(initial));
        transition.completed.insert(0);
        REQUIRE(is_transitively_complete(t, t.root()));
    }
    SECTION("incomplete transition & incomplete node") {
        tree t;
        t.next(t.root(), swarm_state(initial));
        REQUIRE(!is_transitively_complete(t, t.root()));
    }
    SECTION("blocked") {
        tree t;
        t.next(t.root(), swarm_state(blocked));
        REQUIRE(is_transitively_complete(t, t.root()));
    }
}


TEST_CASE("Interleaver - mark complete", "[Interleaver]") {
    const std::vector at_p1 = { thread_state::suspended(p1) };
    const std::vector at_p2 = { thread_state::suspended(p2) };
    const std::vector final = { thread_state::completed() };

    tree t;
    auto& n1 = t.root();
    auto& t1 = t.next(n1, swarm_state(at_p1));
    auto& n2 = t.next(t1, 0);
    auto& t2 = t.next(t.root(), swarm_state(at_p2));
    auto& n3 = t.next(t2, 0);

    mark_complete(t, n3);

    REQUIRE(is_transitively_complete(t, n1));
}


TEST_CASE("Interleaver - select resumed", "[Interleaver]") {
    const std::vector initial = { thread_state::suspended(p3), thread_state::suspended(p3) };
    const std::vector both_ready = { thread_state::suspended(p1), thread_state::suspended(p1) };
    const std::vector left_ready = { thread_state::suspended(p1), thread_state::completed() };
    const std::vector right_ready = { thread_state::completed(), thread_state::suspended(p1) };
    const std::vector none_ready = { thread_state::completed(), thread_state::completed() };
    const std::vector left_blocked = { thread_state::blocked(), thread_state::suspended(p1) };
    const std::vector right_blocked = { thread_state::suspended(p1), thread_state::blocked() };

    tree t;
    auto& transition = t.next(t.root(), swarm_state(initial));

    SECTION("both ready - none visited") {
        REQUIRE(0 == select_resumed(swarm_state(both_ready), transition));
    }
    SECTION("both ready - left visited") {
        t.next(transition, 0);
        REQUIRE(1 == select_resumed(swarm_state(both_ready), transition));
    }
    SECTION("both ready - right visited") {
        t.next(transition, 1);
        REQUIRE(0 == select_resumed(swarm_state(both_ready), transition));
    }
    SECTION("both ready - left completed") {
        t.next(transition, 0);
        t.next(transition, 1);
        transition.completed.insert(0);
        REQUIRE(1 == select_resumed(swarm_state(both_ready), transition));
    }
    SECTION("both ready - right completed") {
        t.next(transition, 0);
        t.next(transition, 1);
        transition.completed.insert(1);
        REQUIRE(0 == select_resumed(swarm_state(both_ready), transition));
    }
    SECTION("both ready - both completed") {
        t.next(transition, 0);
        t.next(transition, 1);
        transition.completed.insert(0);
        transition.completed.insert(1);
        REQUIRE(0 == select_resumed(swarm_state(both_ready), transition));
    }
    SECTION("left ready") {
        REQUIRE(0 == select_resumed(swarm_state(left_ready), transition));
    }
    SECTION("right ready") {
        REQUIRE(1 == select_resumed(swarm_state(right_ready), transition));
    }
    SECTION("left blocked") {
        REQUIRE(1 == select_resumed(swarm_state(left_blocked), transition));
    }
    SECTION("right blocked") {
        REQUIRE(0 == select_resumed(swarm_state(right_blocked), transition));
    }
    SECTION("none ready") {
        REQUIRE_THROWS(select_resumed(swarm_state(none_ready), transition));
    }
}


struct collector_scenario {
    collector_scenario() {
        interleavings.push_back({});
    }

    static void hit(int id) {
        interleavings.back().push_back(id);
    }

    static void reset() {
        interleavings.clear();
    }

    inline static std::vector<std::vector<int>> interleavings = {};
};


TEST_CASE("Interleaver - single thread combinatorics", "[Interleaver]") {
    struct scenario : collector_scenario {
        void thread_0() {
            INTERLEAVED(hit(0));
            INTERLEAVED(hit(1));
            INTERLEAVED(hit(2));
        }
    };

    scenario::reset();
    INTERLEAVED_RUN(
        scenario,
        THREAD("thread_0", &scenario::thread_0));
    const std::vector<std::vector<int>> expected = {
        {0, 1, 2}
    };
    REQUIRE(scenario::interleavings == expected);
}


TEST_CASE("Interleaver - two thread combinatorics", "[Interleaver]") {
    struct scenario : collector_scenario {
        void thread_0() {
            hit(10);
            INTERLEAVED("A0");
            hit(11);
            INTERLEAVED("A1");
            hit(12);
        }
        void thread_1() {
            hit(20);
            INTERLEAVED("B0");
            hit(21);
            INTERLEAVED("B1");
            hit(22);
        }
    };

    scenario::reset();
    INTERLEAVED_RUN(
        scenario,
        THREAD("thread_0", &scenario::thread_0),
        THREAD("thread_1", &scenario::thread_1));

    // Get and sort(!) all executed interleavings.
    auto interleaveings = std::move(scenario::interleavings);
    std::ranges::sort(interleaveings);

    // Check no interleaving was run twice.
    REQUIRE(std::ranges::unique(interleaveings).end() == interleaveings.end());

    // Check we have all the interleavings.
    REQUIRE(interleaveings.size() == ncr(6, 3));
}


TEST_CASE("Interleaver - three thread combinatorics", "[Interleaver]") {
    struct scenario : collector_scenario {
        void thread_0() {
            hit(10);
            INTERLEAVED("A0");
            hit(11);
        }
        void thread_1() {
            hit(20);
            INTERLEAVED("B0");
            hit(21);
        }
        void thread_2() {
            hit(30);
            INTERLEAVED("C0");
            hit(31);
        }
    };

    scenario::reset();
    INTERLEAVED_RUN(
        scenario,
        THREAD("thread_0", &scenario::thread_0),
        THREAD("thread_1", &scenario::thread_1),
        THREAD("thread_2", &scenario::thread_2));

    auto interleaveings = std::move(scenario::interleavings);
    std::ranges::sort(interleaveings);

    REQUIRE(std::ranges::unique(interleaveings).end() == interleaveings.end());
    REQUIRE(interleaveings.size() == ncr(4, 2) * ncr(6, 4));
}


TEST_CASE("Interleaver - acquire", "[Interleaver]") {
    struct scenario : collector_scenario {
        std::atomic_flag f;

        void thread_0() {
            INTERLEAVED_ACQUIRE("A0");
            while (!f.test()) {
            }
            INTERLEAVED("A1");
            hit(10);
        }
        void thread_1() {
            INTERLEAVED("B0");
            hit(20);
            f.test_and_set();
        }
    };

    scenario::reset();
    INTERLEAVED_RUN(
        scenario,
        THREAD("thread_0", &scenario::thread_0),
        THREAD("thread_1", &scenario::thread_1));

    auto interleaveings = std::move(scenario::interleavings);
    std::ranges::sort(interleaveings);
    REQUIRE(interleaveings.size() >= 1);
    REQUIRE(interleaveings[0] == std::vector{ 20, 10 });
}


TEST_CASE("Interleaver - validate", "[Interleaver]") {
    static size_t validations = 0;

    struct scenario : validated_scenario {
        std::atomic_flag flag;

        void thread_0() {
            flag.test_and_set();
        }

        void validate(const path& p) override {
            INFO(p.dump());
            validations++;
        }
    };

    INTERLEAVED_RUN(
        scenario,
        THREAD("thread_0", &scenario::thread_0));

    REQUIRE(validations == 1);
}