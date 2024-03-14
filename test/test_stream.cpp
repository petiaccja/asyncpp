#include "monitor_allocator.hpp"
#include "monitor_task.hpp"

#include <asyncpp/join.hpp>
#include <asyncpp/stream.hpp>

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