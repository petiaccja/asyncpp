#include <asyncpp/container/atomic_deque.hpp>

#include <catch2/catch_test_macros.hpp>


using namespace asyncpp;


struct element {
    element* next;
    element* prev;
};


using deque_t = atomic_deque<element, &element::prev, &element::next>;


TEST_CASE("Atomic deque - empty", "[Atomic deque]") {
    deque_t c;
    REQUIRE(c.front() == nullptr);
    REQUIRE(c.back() == nullptr);
}


TEST_CASE("Atomic deque - push_front", "[Atomic deque]") {
    deque_t c;
    element e1, e2;

    SECTION("single item") {
        c.push_front(&e1);
        REQUIRE(c.front() == &e1);
        REQUIRE(c.back() == &e1);
    }
    SECTION("multiple items") {
        c.push_front(&e1);
        c.push_front(&e2);
        REQUIRE(c.front() == &e2);
        REQUIRE(c.back() == &e1);
    }
}


TEST_CASE("Atomic deque - push_back", "[Atomic deque]") {
    deque_t c;
    element e1, e2;

    SECTION("single item") {
        c.push_back(&e1);
        REQUIRE(c.front() == &e1);
        REQUIRE(c.back() == &e1);
    }
    SECTION("multiple items") {
        c.push_back(&e1);
        c.push_back(&e2);
        REQUIRE(c.front() == &e1);
        REQUIRE(c.back() == &e2);
    }
}


TEST_CASE("Atomic deque - pop_front", "[Atomic deque]") {
    deque_t c;
    element e1, e2, e3;

    c.push_front(&e1);
    c.push_front(&e2);
    c.push_front(&e3);

    REQUIRE(c.pop_front() == &e3);
    REQUIRE(c.front() == &e2);
    REQUIRE(c.back() == &e1);

    REQUIRE(c.pop_front() == &e2);
    REQUIRE(c.front() == &e1);
    REQUIRE(c.back() == &e1);

    REQUIRE(c.pop_front() == &e1);
    REQUIRE(c.front() == nullptr);
    REQUIRE(c.back() == nullptr);
}


TEST_CASE("Atomic deque - pop_back", "[Atomic deque]") {
    deque_t c;
    element e1, e2, e3;

    c.push_back(&e1);
    c.push_back(&e2);
    c.push_back(&e3);

    REQUIRE(c.pop_back() == &e3);
    REQUIRE(c.back() == &e2);
    REQUIRE(c.front() == &e1);

    REQUIRE(c.pop_back() == &e2);
    REQUIRE(c.back() == &e1);
    REQUIRE(c.front() == &e1);

    REQUIRE(c.pop_back() == &e1);
    REQUIRE(c.front() == nullptr);
    REQUIRE(c.back() == nullptr);
}
