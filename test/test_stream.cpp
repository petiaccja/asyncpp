#include "helper_schedulers.hpp"
#include "monitor_allocator.hpp"
#include "monitor_task.hpp"

#include <asyncpp/join.hpp>
#include <asyncpp/stream.hpp>
#include <testing/interleaver.hpp>

#include <catch2/catch_test_macros.hpp>


using namespace asyncpp;


TEST_CASE("Stream: creation", "[Stream]") {
    const auto coro = []() -> stream<int> {
        co_yield 0;
    };

    SECTION("empty") {
        stream<int> s;
        REQUIRE(!s.valid());
    }
    SECTION("valid") {
        auto s = coro();
        REQUIRE(s.valid());
    }
}


TEST_CASE("Stream: iteration", "[Stream]") {
    static const auto coro = []() -> stream<int> {
        co_yield 0;
        co_yield 1;
    };

    auto s = coro();
    auto r = [&]() -> monitor_task {
        auto i1 = co_await s;
        REQUIRE(i1);
        REQUIRE(*i1 == 0);
        auto i2 = co_await s;
        REQUIRE(i2);
        REQUIRE(*i2 == 1);
        auto i3 = co_await s;
        REQUIRE(!i3);
    }();
    REQUIRE(r.get_counters().done);
}


TEST_CASE("Stream: data types", "[Stream]") {
    SECTION("value") {
        static const auto coro = []() -> stream<int> {
            co_yield 0;
        };
        auto r = []() -> monitor_task {
            auto s = coro();
            auto item = co_await s;
            REQUIRE(*item == 0);
        }();
        REQUIRE(r.get_counters().done);
    }
    SECTION("reference") {
        static int value = 0;
        static const auto coro = []() -> stream<int&> {
            co_yield value;
        };
        auto r = []() -> monitor_task {
            auto s = coro();
            auto item = co_await s;
            REQUIRE(&*item == &value);
        }();
        REQUIRE(r.get_counters().done);
    }
    SECTION("moveable") {
        static const auto coro = []() -> stream<std::unique_ptr<int>> {
            co_yield std::make_unique<int>(42);
        };
        auto r = []() -> monitor_task {
            auto s = coro();
            auto item = co_await s;
            REQUIRE(**item == 42);
        }();
        REQUIRE(r.get_counters().done);
    }
    SECTION("exception") {
        static const auto coro = []() -> stream<int> {
            throw std::runtime_error("test");
            co_return;
        };
        auto r = []() -> monitor_task {
            auto s = coro();
            REQUIRE_THROWS_AS(co_await s, std::runtime_error);
        }();
        REQUIRE(r.get_counters().done);
    }
}


TEST_CASE("Stream: destroy", "[Stream]") {
    static const auto coro = []() -> stream<int> { co_yield 0; };

    SECTION("no execution") {
        auto s = coro();
    }
    SECTION("synced") {
        auto s = coro();
        void(join(s));
    }
}


TEST_CASE("Stream: interleaving co_await", "[Stream]") {
    struct scenario {
        thread_locked_scheduler awaiter_sched;
        thread_locked_scheduler awaited_sched;
        stream<int> result;

        scenario() {
            constexpr auto awaited = []() -> stream<int> {
                co_yield 1;
            };
            constexpr auto awaiter = [](stream<int> awaited) -> stream<int> {
                const auto it = co_await awaited;
                const auto value = *it;
                co_yield value;
            };

            auto tmp = launch(awaited(), awaited_sched);
            result = launch(awaiter(std::move(tmp)), awaiter_sched);
        }

        void awaiter() {
            awaiter_sched.resume();
            if (!result.ready()) {
                INTERLEAVED_ACQUIRE(awaiter_sched.wait());
                awaiter_sched.resume();
            }
            REQUIRE(1 == *join(result));
            result = {};
        }

        void awaited() {
            awaited_sched.resume();
        }
    };

    INTERLEAVED_RUN(
        scenario,
        THREAD("awaited", &scenario::awaited),
        THREAD("awaiter", &scenario::awaiter));
}


TEST_CASE("Stream: interleaving abandon", "[Stream]") {
    struct scenario : testing::validated_scenario {
        thread_locked_scheduler sched;
        stream<int> result;

        scenario() {
            result = launch(coro(), sched);
        }

        stream<int> coro() {
            co_yield 1;
        }

        void task() {
            sched.resume();
        }

        void abandon() {
            result = {};
        }

        void validate(const testing::path& path) override {}
    };

    INTERLEAVED_RUN(
        scenario,
        THREAD("task", &scenario::task),
        THREAD("abandon", &scenario::abandon));
}


TEST_CASE("Task: abandon (not started)", "[Task]") {
    static const auto coro = []() -> stream<int> {
        co_return;
    };
    static_cast<void>(coro());
}


TEST_CASE("Task: abandon (mid execution)", "[Task]") {
    static const auto coro = []() -> stream<int> {
        co_yield 1;
        co_yield 2;
        co_return;
    };
    auto s = coro();
    REQUIRE(1 == *join(s));
}


template <class Stream>
auto allocator_free(std::allocator_arg_t, monitor_allocator<>& alloc) -> Stream {
    co_yield alloc;
}


template <class Stream>
struct allocator_object {
    auto member_coro(std::allocator_arg_t, monitor_allocator<>& alloc) -> Stream {
        co_yield alloc;
    }
};


TEST_CASE("Stream: allocator erased", "[Stream]") {
    monitor_allocator<> alloc;
    using stream_t = stream<monitor_allocator<>&>;

    SECTION("free function") {
        auto task = allocator_free<stream_t>(std::allocator_arg, alloc);
        void(*join(task));
        REQUIRE(alloc.get_num_allocations() == 1);
        REQUIRE(alloc.get_num_live_objects() == 0);
    }
    SECTION("member function") {
        allocator_object<stream_t> obj;
        auto task = obj.member_coro(std::allocator_arg, alloc);
        void(*join(task));
        REQUIRE(alloc.get_num_allocations() == 1);
        REQUIRE(alloc.get_num_live_objects() == 0);
    }
}


TEST_CASE("Stream: allocator explicit", "[Stream]") {
    monitor_allocator<> alloc;
    using stream_t = stream<monitor_allocator<>&, monitor_allocator<>>;

    SECTION("free function") {
        auto task = allocator_free<stream_t>(std::allocator_arg, alloc);
        void(*join(task));
        REQUIRE(alloc.get_num_allocations() == 1);
        REQUIRE(alloc.get_num_live_objects() == 0);
    }
    SECTION("member function") {
        allocator_object<stream_t> obj;
        auto task = obj.member_coro(std::allocator_arg, alloc);
        void(*join(task));
        REQUIRE(alloc.get_num_allocations() == 1);
        REQUIRE(alloc.get_num_live_objects() == 0);
    }
}


TEST_CASE("Stream: item operator bool", "[Stream]") {
    SECTION("empty") {
        impl_stream::item<int> item(std::nullopt);
        REQUIRE(!item);
    }
    SECTION("valid") {
        impl_stream::item<int> item(1);
        REQUIRE(!!item);
    }
}


TEST_CASE("Stream: item deref", "[Stream]") {
    impl_stream::item<int> item(1);
    REQUIRE(*item == 1);
}


TEST_CASE("Stream: item arrow", "[Stream]") {
    struct data {
        int value = 0;
    };
    impl_stream::item<data> item(data{ 1 });
    REQUIRE(item->value == 1);
}


TEST_CASE("Stream: item const deref", "[Stream]") {
    const impl_stream::item<int> item(1);
    REQUIRE(*item == 1);
}


TEST_CASE("Stream: item const arrow", "[Stream]") {
    struct data {
        int value = 0;
    };
    const impl_stream::item<data> item(data{ 1 });
    REQUIRE(item->value == 1);
}