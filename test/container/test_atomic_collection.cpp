#include <asyncpp/container/atomic_collection.hpp>
#include <asyncpp/testing/interleaver.hpp>

#include <catch2/catch_test_macros.hpp>


using namespace asyncpp;


struct collection_element {
    int id = 0;
    collection_element* next = nullptr;
};

using collection_t = atomic_collection<collection_element, &collection_element::next>;


TEST_CASE("Atomic collection: empty", "[Atomic collection]") {
    collection_t collection;
    REQUIRE(collection.empty());
    REQUIRE(collection.first() == nullptr);
    REQUIRE(!collection.closed());
}


TEST_CASE("Atomic collection: push", "[Atomic collection]") {
    collection_t collection;
    collection_element e1{ 1 };
    collection_element e2{ 2 };
    REQUIRE(nullptr == collection.push(&e1));
    REQUIRE(&e1 == collection.first());
    REQUIRE(&e1 == collection.push(&e2));
    REQUIRE(&e2 == collection.first());
    REQUIRE(!collection.empty());
    REQUIRE(!collection.closed());
}


TEST_CASE("Atomic collection: detach", "[Atomic collection]") {
    collection_t collection;
    collection_element e1{ 1 };
    collection_element e2{ 2 };

    collection.push(&e1);
    collection.push(&e2);

    const collection_element* detached = collection.detach();
    REQUIRE(collection.empty());
    REQUIRE(collection.first() == nullptr);
    REQUIRE(!collection.closed());

    REQUIRE(detached == &e2);
    REQUIRE(detached->next == &e1);
    REQUIRE(detached->next->next == nullptr);
}


TEST_CASE("Atomic collection: close", "[Atomic collection]") {
    collection_t collection;
    collection_element e1{ 1 };
    collection_element e2{ 2 };

    collection.push(&e1);
    collection.push(&e2);

    const collection_element* detached = collection.close();
    REQUIRE(collection.empty());
    REQUIRE(collection.closed());

    REQUIRE(detached == &e2);
    REQUIRE(detached->next == &e1);
    REQUIRE(detached->next->next == nullptr);
}


TEST_CASE("Atomic collection: push to closed", "[Atomic collection]") {
    collection_t collection;
    collection_element e1{ 1 };
    collection.close();
    REQUIRE(collection_t::closed(collection.push(&e1)));
    REQUIRE(collection.closed());
}


TEST_CASE("Atomic collection: push-push interleave", "[Atomic collection]") {
    struct scenario : testing::validated_scenario {
        collection_t collection;
        collection_element e1{ 1 };
        collection_element e2{ 2 };

        void thread_1() {
            collection.push(&e1);
        }

        void thread_2() {
            collection.push(&e2);
        }

        void validate(const testing::path& p) override {
            INFO(p.dump());
            size_t size = 0;
            collection_element* first = collection.first();
            bool e1_found = false;
            bool e2_found = false;
            while (first) {
                if (first == &e1) {
                    e1_found = true;
                }
                if (first == &e2) {
                    e2_found = true;
                }
                ++size;
                first = first->next;
            }
            REQUIRE(size == 2);
            REQUIRE(e1_found);
            REQUIRE(e2_found);
        }
    };

    INTERLEAVED_RUN(scenario, THREAD("t1", &scenario::thread_1), THREAD("t2", &scenario::thread_2));
}


TEST_CASE("Atomic collection: push-detach interleave", "[Atomic collection]") {
    struct scenario : testing::validated_scenario {
        collection_t collection;
        collection_element e1{ 1 };
        collection_element* volatile detached = nullptr;

        void thread_1() {
            collection.push(&e1);
        }

        void thread_2() {
            detached = collection.detach();
        }

        void validate(const testing::path& p) override {
            INFO(p.dump());
            if (detached == nullptr) {
                REQUIRE(collection.first() == &e1);
            }
            else {
                REQUIRE(collection.empty());
                REQUIRE(detached == &e1);
            }
        }
    };

    INTERLEAVED_RUN(scenario, THREAD("t1", &scenario::thread_1), THREAD("t2", &scenario::thread_2));
}


TEST_CASE("Atomic collection: push-close interleave", "[Atomic collection]") {
    struct scenario : testing::validated_scenario {
        collection_t collection;
        collection_element e1{ 1 };
        collection_element* volatile closed = nullptr;

        void thread_1() {
            collection.push(&e1);
        }

        void thread_2() {
            closed = collection.close();
        }

        void validate(const testing::path& p) override {
            INFO(p.dump());
            if (closed == nullptr) {
                REQUIRE(collection.closed());
            }
            else {
                REQUIRE(collection.closed());
                REQUIRE(closed == &e1);
            }
        }
    };

    INTERLEAVED_RUN(scenario, THREAD("t1", &scenario::thread_1), THREAD("t2", &scenario::thread_2));
}