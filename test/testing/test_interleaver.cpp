#include <asyncpp/testing/interleaver.hpp>

#include <catch2/catch_test_macros.hpp>


using namespace asyncpp::testing;


static suspension_point p1;
static suspension_point p2;
static suspension_point p3;


TEST_CASE("Interleaver - select resumed") {
    const interleaving_graph::vertex start_state({
        thread_state::suspended(p1),
        thread_state::suspended(p1),
    });

    const interleaving_graph::vertex on_0_state({
        thread_state::suspended(p2),
        thread_state::suspended(p1),
    });

    const interleaving_graph::vertex on_1_state({
        thread_state::suspended(p1),
        thread_state::suspended(p2),
    });

    const interleaving_graph::vertex blocked_state({
        thread_state::blocked,
        thread_state::suspended(p1),
        thread_state::blocked,
        thread_state::blocked,
    });

    const interleaving_graph::vertex completed_state({
        thread_state::completed,
        thread_state::completed,
        thread_state::suspended(p1),
        thread_state::completed,
    });

    const interleaving_graph::vertex final_state({
        thread_state::completed,
        thread_state::completed,
    });

    SECTION("unconstrained") {
        interleaving_graph_state graph;
        graph.graph.add_vertex(start_state);
        REQUIRE(0 == select_resumed(graph, start_state));
    }
    SECTION("completion constrained") {
        interleaving_graph_state graph;
        graph.graph.add_vertex(start_state);
        graph.graph.add_vertex(on_0_state);
        graph.graph.add_vertex(on_1_state);
        graph.graph.add_edge(start_state, on_0_state);
        graph.graph.add_edge(start_state, on_1_state);
        graph.transition_map[{ start_state, on_0_state }] = 0;
        graph.transition_map[{ start_state, on_1_state }] = 1;

        SECTION("0th incomplete") {
            graph.completion_map[on_0_state] = false;
            graph.completion_map[on_1_state] = true;
            REQUIRE(0 == select_resumed(graph, start_state));
        }

        SECTION("1th incomplete") {
            graph.completion_map[on_0_state] = true;
            graph.completion_map[on_1_state] = false;
            REQUIRE(1 == select_resumed(graph, start_state));
        }

        SECTION("both complete") {
            graph.completion_map[on_0_state] = true;
            graph.completion_map[on_1_state] = true;
            REQUIRE(0 == select_resumed(graph, start_state));
        }
    }
    SECTION("blocked") {
        interleaving_graph_state graph;
        graph.graph.add_vertex(blocked_state);
        REQUIRE(1 == select_resumed(graph, blocked_state));
    }
    SECTION("completed") {
        interleaving_graph_state graph;
        graph.graph.add_vertex(completed_state);
        REQUIRE(2 == select_resumed(graph, completed_state));
    }
    SECTION("final") {
        interleaving_graph_state graph;
        graph.graph.add_vertex(final_state);
        REQUIRE_THROWS(select_resumed(graph, final_state));
    }
}


TEST_CASE("Interleaver - mark completed tree dense") {
    const interleaving_graph::vertex start_state({
        thread_state::suspended(p1),
        thread_state::suspended(p1),
    });

    const interleaving_graph::vertex left_state({
        thread_state::suspended(p2),
        thread_state::suspended(p1),
    });

    const interleaving_graph::vertex right_state({
        thread_state::suspended(p1),
        thread_state::suspended(p2),
    });

    interleaving_graph_state graph;
    graph.graph.add_vertex(start_state);
    graph.graph.add_vertex(left_state);
    graph.graph.add_vertex(right_state);
    graph.graph.add_edge(start_state, left_state);
    graph.graph.add_edge(start_state, right_state);
    graph.completion_map[start_state] = false;
    graph.completion_map[left_state] = false;
    graph.completion_map[right_state] = false;

    SECTION("mark left") {
        mark_completed(graph, left_state);
        REQUIRE(graph.completion_map[start_state] == false);
        REQUIRE(graph.completion_map[left_state] == true);
        REQUIRE(graph.completion_map[right_state] == false);
    }
    SECTION("mark right") {
        mark_completed(graph, right_state);
        REQUIRE(graph.completion_map[start_state] == false);
        REQUIRE(graph.completion_map[left_state] == false);
        REQUIRE(graph.completion_map[right_state] == true);
    }
    SECTION("mark both") {
        mark_completed(graph, left_state);
        mark_completed(graph, right_state);
        REQUIRE(graph.completion_map[start_state] == true);
        REQUIRE(graph.completion_map[left_state] == true);
        REQUIRE(graph.completion_map[right_state] == true);
    }
}


TEST_CASE("Interleaver - mark completed tree sparse") {
    const interleaving_graph::vertex start_state({
        thread_state::suspended(p1),
        thread_state::suspended(p1),
    });

    const interleaving_graph::vertex left_state({
        thread_state::suspended(p2),
        thread_state::suspended(p1),
    });

    interleaving_graph_state graph;
    graph.graph.add_vertex(start_state);
    graph.graph.add_vertex(left_state);
    graph.graph.add_edge(start_state, left_state);
    graph.completion_map[start_state] = false;
    graph.completion_map[left_state] = false;

    mark_completed(graph, left_state);
    REQUIRE(graph.completion_map[start_state] == false);
    REQUIRE(graph.completion_map[left_state] == true);
}


TEST_CASE("Interleaver - mark completed tree leaf") {
    const interleaving_graph::vertex start_state({
        thread_state::completed,
        thread_state::suspended(p1),
    });

    const interleaving_graph::vertex left_state({
        thread_state::completed,
        thread_state::completed,
    });

    interleaving_graph_state graph;
    graph.graph.add_vertex(start_state);
    graph.graph.add_vertex(left_state);
    graph.graph.add_edge(start_state, left_state);
    graph.completion_map[start_state] = false;
    graph.completion_map[left_state] = false;

    mark_completed(graph, left_state);
    REQUIRE(graph.completion_map[start_state] == true);
    REQUIRE(graph.completion_map[left_state] == true);
}


TEST_CASE("Interleaver - mark completed circle") {
    const interleaving_graph::vertex state_0({
        thread_state::suspended(p1),
    });

    const interleaving_graph::vertex state_r({
        thread_state::suspended(p3),
    });

    const interleaving_graph::vertex state_1({
        thread_state::suspended(p2),
    });

    const interleaving_graph::vertex state_2({
        thread_state::completed,
    });

    interleaving_graph_state graph;
    graph.graph.add_vertex(state_0);
    graph.graph.add_vertex(state_r);
    graph.graph.add_vertex(state_1);
    graph.graph.add_vertex(state_2);
    graph.graph.add_edge(state_0, state_1);
    graph.graph.add_edge(state_1, state_r);
    graph.graph.add_edge(state_r, state_0);
    graph.graph.add_edge(state_1, state_2);
    graph.completion_map[state_0] = false;
    graph.completion_map[state_r] = false;
    graph.completion_map[state_1] = false;
    graph.completion_map[state_2] = false;

    mark_completed(graph, state_2);
    REQUIRE(graph.completion_map[state_0] == true);
    REQUIRE(graph.completion_map[state_r] == true);
    REQUIRE(graph.completion_map[state_1] == true);
    REQUIRE(graph.completion_map[state_2] == true);
}


struct CollectorScenario {
    CollectorScenario() {
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
    struct Scenario : CollectorScenario {
        static void thread_0(Scenario&) {
            INTERLEAVED(hit(0));
            INTERLEAVED(hit(1));
            INTERLEAVED(hit(2));
        }
    };

    Scenario::reset();
    INTERLEAVED_TEST(
        Scenario,
        INTERLEAVED_THREAD("thread_0", &Scenario::thread_0));
    const std::vector<std::vector<int>> expected = {
        {0, 1, 2}
    };
    REQUIRE(Scenario::interleavings == expected);
}


TEST_CASE("Interleaver - two thread combinatorics", "[Interleaver]") {
    struct Scenario : CollectorScenario {
        static void thread_0(Scenario&) {
            hit(0);
            INTERLEAVED("A0");
            hit(1);
            INTERLEAVED("A1");
            hit(2);
        }
        static void thread_1(Scenario&) {
            hit(3);
            INTERLEAVED("B0");
            hit(4);
            INTERLEAVED("B1");
            hit(5);
        }
    };

    Scenario::reset();
    INTERLEAVED_TEST(
        Scenario,
        INTERLEAVED_THREAD("thread_0", &Scenario::thread_0),
        INTERLEAVED_THREAD("thread_1", &Scenario::thread_1));
    std::vector<std::vector<int>> expected = {
        {0,  1, 2, 3, 4, 5},
        { 0, 1, 3, 2, 4, 5},
        { 0, 1, 3, 4, 2, 5},
        { 0, 1, 3, 4, 5, 2},
        { 0, 3, 1, 2, 4, 5},
        { 0, 3, 1, 4, 2, 5},
        { 0, 3, 1, 4, 5, 2},
        { 0, 3, 4, 1, 2, 5},
        { 0, 3, 4, 1, 5, 2},
        { 0, 3, 4, 5, 1, 2},
        { 3, 0, 4, 5, 1, 2},
        { 3, 4, 0, 5, 1, 2},
        { 3, 4, 5, 0, 1, 2},
    };
    std::ranges::sort(expected);
    std::ranges::sort(Scenario::interleavings);
    REQUIRE(Scenario::interleavings == expected);
}