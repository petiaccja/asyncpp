#include <async++/interleaving/runner.hpp>
#include <async++/interleaving/sequence_point.hpp>
#include <async++/task.hpp>

#include <functional>

#include <catch2/catch_test_macros.hpp>


using namespace asyncpp;


TEST_CASE("Sequence point: only initial points", "[Sequence point]") {
    const auto func1 = [] {};
    const auto func2 = [] {};
    size_t count = 0;
    for (auto interleaving_ : interleaving::run_all({ func1, func2 })) {
        std::cout << interleaving::interleaving_printer{ interleaving_ } << std::endl;
        ++count;
    }
    REQUIRE(count == 2);
}


void test_func_1() {
    SEQUENCE_POINT("a");
    SEQUENCE_POINT("b");
    SEQUENCE_POINT("c");
}


void test_func_2() {
    SEQUENCE_POINT("d");
}


TEST_CASE("Sequence point: linear multiple points", "[Sequence point]") {
    size_t count = 0;
    for (auto interleaving_ : interleaving::run_all({ &test_func_1, &test_func_2 })) {
        std::cout << interleaving::interleaving_printer{ interleaving_ } << std::endl;
        ++count;
    }
    REQUIRE(count == 15);
}


void test_func_branch_1(std::shared_ptr<std::atomic_bool> cond) {
    if (cond->load()) {
        SEQUENCE_POINT("b_true");
    }
    else {
        SEQUENCE_POINT("b_false");
    }
}


void test_func_branch_2(std::shared_ptr<std::atomic_bool> cond) {
    cond->store(true);
}


TEST_CASE("Sequence point: branching", "[Sequence point]") {
    auto fixture = std::function([] { return std::make_shared<std::atomic_bool>(false); });
    size_t count = 0;
    for (auto interleaving_ : interleaving::run_all(fixture, std::vector{ std::function(test_func_branch_1), std::function(test_func_branch_2) })) {
        std::cout << interleaving::interleaving_printer{ interleaving_ } << std::endl;
        ++count;
    }
    REQUIRE(count == 3);
}
