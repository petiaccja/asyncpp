#include "helper_schedulers.hpp"

#include <asyncpp/join.hpp>
#include <asyncpp/task.hpp>
#include <asyncpp/testing/interleaver.hpp>
#include <asyncpp/thread_pool.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>

#include <catch2/catch_test_macros.hpp>


using namespace asyncpp;

constexpr size_t num_threads = 4;
constexpr int64_t depth = 5;
constexpr size_t branching = 10;


struct test_promise : schedulable_promise {
    void resume_now() final {
        ++num_queried;
    }
    std::atomic_size_t num_queried = 0;
};


TEST_CASE("Thread pool 3: insert - try_get_promise interleave", "[Thread pool 3]") {
    struct scenario : testing::validated_scenario {
        thread_pool::pack pack{ .workers = std::vector<thread_pool::worker>(1) };
        test_promise promise;
        bool exit_loop = false;
        size_t stealing_attempt = 0;
        schedulable_promise* result = nullptr;

        void insert() {
            pack.workers[0].insert(promise);
        }

        void try_get_promise() {
            result = pack.workers[0].try_get_promise(pack, stealing_attempt, exit_loop);
        }

        void validate(const testing::path& p) override {
            INFO(p.dump());
            if (!result) {
                stealing_attempt = 1;
                result = pack.workers[0].try_get_promise(pack, stealing_attempt, exit_loop);
            }
            REQUIRE(result == &promise);
            REQUIRE(exit_loop == false);
        }
    };

    INTERLEAVED_RUN(scenario, THREAD("insert", &scenario::insert), THREAD("get", &scenario::try_get_promise));
}


TEST_CASE("Thread pool 3: cancel - try_get_promise interleave", "[Thread pool 3]") {
    struct scenario : testing::validated_scenario {
        thread_pool::pack pack{ .workers = std::vector<thread_pool::worker>(1) };
        bool exit_loop = false;
        size_t stealing_attempt = 0;
        schedulable_promise* result = nullptr;

        void insert() {
            pack.workers[0].cancel();
        }

        void try_get_promise() {
            pack.workers[0].try_get_promise(pack, stealing_attempt, exit_loop);
        }

        void validate(const testing::path& p) override {
            INFO(p.dump());
            REQUIRE(result == nullptr);
            if (!exit_loop) {
                stealing_attempt = 0;
                pack.workers[0].try_get_promise(pack, stealing_attempt, exit_loop);
            }
            REQUIRE(exit_loop == true);
        }
    };

    INTERLEAVED_RUN(scenario, THREAD("cancel", &scenario::insert), THREAD("get", &scenario::try_get_promise));
}


TEST_CASE("Thread pool 3: steal - try_get_promise interleave", "[Thread pool 3]") {
    struct scenario : testing::validated_scenario {
        thread_pool::pack pack{ .workers = std::vector<thread_pool::worker>(1) };
        test_promise promise;
        bool exit_loop = false;
        size_t stealing_attempt = 0;
        schedulable_promise* popped = nullptr;
        schedulable_promise* stolen = nullptr;

        scenario() {
            pack.workers[0].insert(promise);
        }

        void steal() {
            stolen = pack.workers[0].steal_from_this();
            pack.workers[0].cancel();
        }

        void try_get_promise() {
            popped = pack.workers[0].try_get_promise(pack, stealing_attempt, exit_loop);
        }

        void validate(const testing::path& p) override {
            INFO(p.dump());
            REQUIRE((!!popped ^ !!stolen));
        }
    };

    INTERLEAVED_RUN(scenario, THREAD("cancel", &scenario::steal), THREAD("get", &scenario::try_get_promise));
}


TEST_CASE("Thread pool 3: smoke test - schedule tasks", "[Scheduler]") {
    thread_pool sched(num_threads);

    static const auto coro = [&sched](auto self, int depth) -> task<int64_t> {
        if (depth <= 0) {
            co_return 1;
        }
        std::array<task<int64_t>, branching> children;
        std::ranges::generate(children, [&] { return launch(self(self, depth - 1), sched); });
        int64_t sum = 0;
        for (auto& tk : children) {
            sum += co_await tk;
        }
        co_return sum;
    };

    const auto count = int64_t(std::pow(branching, depth));
    const auto result = join(bind(coro(coro, depth), sched));
    REQUIRE(result == count);
}
