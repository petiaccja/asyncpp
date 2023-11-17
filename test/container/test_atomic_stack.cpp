#include <async++/container/atomic_stack.hpp>

#include <catch2/catch_test_macros.hpp>


using namespace asyncpp;


struct queue_element {
    int id = 0;
    queue_element* next = nullptr;
};

using queue_t = atomic_stack<queue_element, &queue_element::next>;


TEST_CASE("Atomic stack: all", "[Atomic stack]") {
    queue_element e0{ .id = 0 };
    queue_element e1{ .id = 1 };
    queue_element e2{ .id = 2 };
    queue_element e3{ .id = 3 };
    queue_t stack;
    REQUIRE(stack.empty());
    REQUIRE(stack.pop() == nullptr);
    REQUIRE(stack.push(&e0) == nullptr);
    REQUIRE(stack.push(&e1) == &e0);
    REQUIRE(stack.push(&e2) == &e1);
    REQUIRE(stack.push(&e3) == &e2);
    REQUIRE(!stack.empty());
    REQUIRE(stack.pop() == &e3);
    REQUIRE(stack.pop() == &e2);
    REQUIRE(stack.pop() == &e1);
    REQUIRE(stack.pop() == &e0);
    REQUIRE(stack.empty());
    REQUIRE(stack.pop() == nullptr);
}