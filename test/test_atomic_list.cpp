#include <async++/atomic_list.hpp>

#include <catch2/catch_test_macros.hpp>


using namespace asyncpp;


struct list_element {
    int id = 0;
    list_element* next = nullptr;
};

using list_t = atomic_list<list_element, &list_element::next>;


TEST_CASE("Atomic list: all", "[Atomic list]") {
    list_element e0{ .id = 0 };
    list_element e1{ .id = 1 };
    list_element e2{ .id = 2 };
    list_element e3{ .id = 3 };
    list_t list;
    REQUIRE(list.empty());
    REQUIRE(list.pop() == nullptr);
    REQUIRE(list.push(&e0) == nullptr);
    REQUIRE(list.push(&e1) == &e0);
    REQUIRE(list.push(&e2) == &e1);
    REQUIRE(list.push(&e3) == &e2);
    REQUIRE(!list.empty());
    REQUIRE(list.pop() == &e3);
    REQUIRE(list.pop() == &e2);
    REQUIRE(list.pop() == &e1);
    REQUIRE(list.pop() == &e0);
    REQUIRE(list.empty());
    REQUIRE(list.pop() == nullptr);
}