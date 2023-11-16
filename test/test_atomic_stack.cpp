#include <async++/sync/atomic_stack.hpp>

#include <catch2/catch_test_macros.hpp>


using namespace asyncpp;


struct collection_element {
    int id = 0;
    collection_element* next = nullptr;
};

using stack_t = atomic_stack<collection_element, &collection_element::next>;


TEST_CASE("Atomic list: all", "[Atomic list]") {
    collection_element e0{ .id = 0 };
    collection_element e1{ .id = 1 };
    collection_element e2{ .id = 2 };
    collection_element e3{ .id = 3 };
    stack_t stack;
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