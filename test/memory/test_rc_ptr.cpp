#include <asyncpp/memory/rc_ptr.hpp>

#include <catch2/catch_test_macros.hpp>


using namespace asyncpp;


struct managed : rc_from_this {
    void destroy() {
        ++destroyed;
    }
    size_t destroyed = 0;
};


TEST_CASE("Refcounted pointer - empty", "[Refcounted pointer]") {
    rc_ptr<managed> ptr;
    REQUIRE(!ptr);
    REQUIRE(ptr.use_count() == 0);
    REQUIRE(ptr.unique() == false);
    REQUIRE(ptr.get() == nullptr);
}


TEST_CASE("Refcounted pointer - unique", "[Refcounted pointer]") {
    managed object;
    {
        rc_ptr ptr(&object);
        REQUIRE(ptr);
        REQUIRE(ptr.use_count() == 1);
        REQUIRE(ptr.unique() == true);
        REQUIRE(ptr.get() == &object);
    }
    REQUIRE(object.destroyed == 1);
}


TEST_CASE("Refcounted pointer - multiple", "[Refcounted pointer]") {
    managed object;
    {
        rc_ptr ptr1(&object);
        rc_ptr ptr2(&object);

        REQUIRE(ptr1.use_count() == 2);
        REQUIRE(ptr1.unique() == false);
        REQUIRE(ptr1.get() == &object);

        REQUIRE(ptr2.use_count() == 2);
        REQUIRE(ptr2.unique() == false);
        REQUIRE(ptr2.get() == &object);

        REQUIRE(ptr1 == ptr2);
    }
    REQUIRE(object.destroyed == 1);
}


TEST_CASE("Refcounted pointer - move construct", "[Refcounted pointer]") {
    managed object;
    {
        rc_ptr ptr(&object);
        rc_ptr copy(std::move(ptr));

        REQUIRE(copy);
        REQUIRE(copy.use_count() == 1);
        REQUIRE(copy.unique() == true);
        REQUIRE(copy.get() == &object);
    }
    REQUIRE(object.destroyed == 1);
}


TEST_CASE("Refcounted pointer - copy construct", "[Refcounted pointer]") {
    managed object;
    {
        rc_ptr ptr(&object);
        rc_ptr copy(ptr);

        REQUIRE(copy);
        REQUIRE(copy.use_count() == 2);
        REQUIRE(copy.unique() == false);
        REQUIRE(copy.get() == &object);
    }
    REQUIRE(object.destroyed == 1);
}


TEST_CASE("Refcounted pointer - move assign", "[Refcounted pointer]") {
    managed object;
    {
        rc_ptr ptr(&object);
        rc_ptr<managed> copy;
        copy = std::move(ptr);

        REQUIRE(copy);
        REQUIRE(copy.use_count() == 1);
        REQUIRE(copy.unique() == true);
        REQUIRE(copy.get() == &object);
    }
    REQUIRE(object.destroyed == 1);
}


TEST_CASE("Refcounted pointer - copy assign", "[Refcounted pointer]") {
    managed object;
    {
        rc_ptr ptr(&object);
        rc_ptr<managed> copy;
        copy = ptr;

        REQUIRE(copy);
        REQUIRE(copy.use_count() == 2);
        REQUIRE(copy.unique() == false);
        REQUIRE(copy.get() == &object);
    }
    REQUIRE(object.destroyed == 1);
}


TEST_CASE("Refcounted pointer - dereference", "[Refcounted pointer]") {
    managed object;
    object.destroyed = 10;
    rc_ptr ptr(&object);
    REQUIRE(ptr->destroyed == 10);
    REQUIRE((*ptr).destroyed == 10);
    REQUIRE(ptr.get()->destroyed == 10);
}