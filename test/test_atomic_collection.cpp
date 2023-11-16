#include <async++/sync/atomic_collection.hpp>

#include <catch2/catch_test_macros.hpp>


using namespace asyncpp;


struct collection_element {
    int id = 0;
    collection_element* next = nullptr;
};

using collection_t = atomic_collection<collection_element, &collection_element::next>;


TEST_CASE("Atomic collection: empty on creation", "[Atomic collection]") {
    collection_t collection;
    REQUIRE(collection.empty());
    REQUIRE(!collection.closed());
}


TEST_CASE("Atomic collection: push once", "[Atomic collection]") {
    collection_t collection;
    collection_element e1{ 1 };
    REQUIRE(nullptr == collection.push(&e1));
    REQUIRE(!collection.empty());
    REQUIRE(!collection.closed());
}


TEST_CASE("Atomic collection: detach", "[Atomic collection]") {
    collection_t collection;
    collection_element e1{ 1 };
    collection_element e2{ 2 };
    REQUIRE(nullptr == collection.push(&e1));
    REQUIRE(&e1 == collection.push(&e2));
    const collection_element* detached = collection.detach();
    REQUIRE(collection.empty());
    REQUIRE(detached == &e2);
    REQUIRE(detached->next == &e1);
    REQUIRE(detached->next->next == nullptr);
    REQUIRE(nullptr == collection.push(&e1));
    REQUIRE(!collection.empty());
}


TEST_CASE("Atomic collection: close", "[Atomic collection]") {
    collection_t collection;
    collection_element e1{ 1 };
    collection_element e2{ 2 };
    REQUIRE(nullptr == collection.push(&e1));
    REQUIRE(&e1 == collection.push(&e2));
    const collection_element* detached = collection.close();
    REQUIRE(collection.empty());
    REQUIRE(collection.closed());
    REQUIRE(detached == &e2);
    REQUIRE(detached->next == &e1);
    REQUIRE(detached->next->next == nullptr);
    REQUIRE(collection.closed(collection.push(&e1)));
    REQUIRE(collection.empty());
    REQUIRE(collection.closed());
}