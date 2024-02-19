#include <asyncpp/container/atomic_item.hpp>
#include <asyncpp/testing/interleaver.hpp>

#include <catch2/catch_test_macros.hpp>


using namespace asyncpp;


struct element {
    int id = 0;
};

using item_t = atomic_item<element>;


TEST_CASE("Atomic item: empty", "[Atomic item]") {
    item_t item;
    REQUIRE(item.empty());
    REQUIRE(item.item() == nullptr);
    REQUIRE(!item.closed());
}


TEST_CASE("Atomic item: set", "[Atomic item]") {
    item_t item;
    element e{ 1 };
    item.set(&e);
    REQUIRE(item.item() == &e);
    REQUIRE(!item.empty());
    REQUIRE(!item.closed());
}


TEST_CASE("Atomic item: set twice", "[Atomic item]") {
    item_t item;
    element e1{ 1 };
    element e2{ 2 };
    item.set(&e1);
    REQUIRE(&e1 == item.set(&e2));
    REQUIRE(item.item() == &e1);
    REQUIRE(!item.empty());
    REQUIRE(!item.closed());
}


TEST_CASE("Atomic item: set closed", "[Atomic item]") {
    item_t item;
    item.close();
    element e1{ 1 };
    REQUIRE(item_t::closed(item.set(&e1)));
    REQUIRE(item.closed());
}


TEST_CASE("Atomic item: close", "[Atomic item]") {
    item_t item;
    element e1{ 1 };
    item.set(&e1);
    REQUIRE(item.close() == &e1);
    REQUIRE(item.empty());
    REQUIRE(item.closed());
}


TEST_CASE("Atomic item: push-push interleave", "[Atomic item]") {
    struct scenario : testing::validated_scenario {
        item_t item;
        element e1{ 1 };
        element e2{ 2 };

        void thread_1() {
            item.set(&e1);
        }

        void thread_2() {
            item.set(&e2);
        }

        void validate(const testing::path& p) override {
            INFO(p.dump());
            REQUIRE((item.item() == &e1 || item.item() == &e2));
        }
    };

    INTERLEAVED_RUN(scenario, THREAD("t1", &scenario::thread_1), THREAD("t2", &scenario::thread_2));
}


TEST_CASE("Atomic item: push-close interleave", "[Atomic item]") {
    struct scenario: testing::validated_scenario  {
        item_t item;
        element e{ 1 };
        element* volatile closed = nullptr;

        void thread_1() {
            item.set(&e);
        }

        void thread_2() {
            closed = item.close();
        }

        void validate(const testing::path& p) override {
            INFO(p.dump());
            if (closed == nullptr) {
                REQUIRE(item.closed());
            }
            else {
                REQUIRE(item.closed());
                REQUIRE(closed == &e);
            }
        }
    };

    INTERLEAVED_RUN(scenario, THREAD("t1", &scenario::thread_1), THREAD("t2", &scenario::thread_2));
}