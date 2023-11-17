#include <async++/container/atomic_queue.hpp>

#include <catch2/catch_test_macros.hpp>


using namespace asyncpp;


struct queue_element {
    int id = 0;
    queue_element* next = nullptr;
    queue_element* prev = nullptr;
};

using queue_t = atomic_queue<queue_element, &queue_element::next, &queue_element::prev>;


TEST_CASE("Atomic queue: all", "[Atomic queue]") {
    queue_element e0{ .id = 0 };
    queue_element e1{ .id = 1 };
    queue_element e2{ .id = 2 };
    queue_element e3{ .id = 3 };
    queue_t queue;
    REQUIRE(queue.empty());
    REQUIRE(queue.pop() == nullptr);
    REQUIRE(queue.push(&e0) == nullptr);
    REQUIRE(queue.push(&e1) == &e0);
    REQUIRE(queue.push(&e2) == &e1);
    REQUIRE(queue.push(&e3) == &e2);
    REQUIRE(!queue.empty());
    REQUIRE(queue.pop() == &e0);
    REQUIRE(queue.pop() == &e1);
    REQUIRE(queue.pop() == &e2);
    REQUIRE(queue.pop() == &e3);
    REQUIRE(queue.empty());
    REQUIRE(queue.pop() == nullptr);
}