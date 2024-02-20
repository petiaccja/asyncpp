#include <asyncpp/container/atomic_stack.hpp>

#include <catch2/catch_test_macros.hpp>


using namespace asyncpp;


struct element {
    element* next;
    element* prev;
};


using stack_t = atomic_stack<element, &element::next>;


TEST_CASE("Atomic stack: empty", "[Atomic stack]") {
    stack_t c;
    REQUIRE(c.top() == nullptr);
    REQUIRE(c.empty());
}


TEST_CASE("Atomic stack - push", "[Atomic stack]") {
    stack_t c;
    element e1, e2;

    c.push(&e1);
    REQUIRE(c.top() == &e1);
    c.push(&e2);
    REQUIRE(c.top() == &e2);
}


TEST_CASE("Atomic stack - pop", "[Atomic stack]") {
    stack_t c;
    element e1, e2;

    c.push(&e1);
    c.push(&e2);

    REQUIRE(c.pop() == &e2);
    REQUIRE(c.pop() == &e1);
    REQUIRE(c.pop() == nullptr);
}