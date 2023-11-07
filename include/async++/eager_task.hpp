#pragma once

#include "task.hpp"

#include <cassert>
#include <coroutine>
#include <future>
#include <optional>


namespace asyncpp {


template <class T>
class task;


namespace impl_eager_task {

    using namespace impl;
    using impl_task::promise_result;


    template <class T>
    struct awaitable_final;


    template <class T>
    struct promise : promise_result<T>, resumable_promise, schedulable_promise {
        static constexpr auto INITIAL = static_cast<basic_awaitable<T>*>(nullptr);
        static inline const auto DONE = reinterpret_cast<basic_awaitable<T>*>(std::numeric_limits<size_t>::max());
        std::atomic<basic_awaitable<T>*> m_awaiting = INITIAL;
        std::atomic_flag m_resumed_once;

        auto get_return_object() { return eager_task<T>(this); }
        constexpr auto initial_suspend() const noexcept { return std::suspend_always{}; }
        auto final_suspend() noexcept { return awaitable_final<T>{}; }
        auto handle() -> std::coroutine_handle<> override { return std::coroutine_handle<promise>::from_promise(*this); }
        void resume() override { return m_scheduler ? m_scheduler->schedule(*this) : handle().resume(); }
        void resume_once() { return m_resumed_once.test_and_set() ? void() : resume(); }
        task_result<T> get_result() { return std::move(this->m_result); }
        bool set_awaiting(basic_awaitable<T>* awaiting) noexcept {
            const auto await_state = m_awaiting.exchange(awaiting);
            if (await_state == INITIAL) return false; // Results not ready.
            if (await_state == DONE) return true; // Results ready.
            assert(false && "only one object should be awaiting the promise");
            std::terminate();
        }
    };


    template <class T>
    struct awaitable_final {
        constexpr bool await_ready() const noexcept { return false; }
        bool await_suspend(std::coroutine_handle<promise<T>> handle) const noexcept {
            auto& pr = handle.promise();
            const auto await_state = pr.m_awaiting.exchange(pr.DONE);
            if (await_state == pr.DONE) {
                return false; // Lets the coroutine continue and destroy itself.
            }
            if (await_state == pr.INITIAL) {
                return true; // Stop the coroutine and wait for owner to retrieve results.
            }
            // Stop the coroutine and continue owner.
            await_state->set_results(pr.get_result());
            return true;
        }
        constexpr void await_resume() const noexcept {}
    };


    template <class T>
    struct awaitable : basic_awaitable<T> {
        task_result<T> m_result;
        promise<T>* m_awaited = nullptr;
        resumable_promise* m_enclosing = nullptr;

        awaitable(promise<T>* awaited) : m_awaited(awaited) {}
        bool await_ready() const noexcept { return false; }
        template <std::convertible_to<const resumable_promise&> Promise>
        bool await_suspend(std::coroutine_handle<Promise> enclosing) {
            m_enclosing = &enclosing.promise();
            m_awaited->resume_once();
            if ([[maybe_unused]] const bool ready = m_awaited->set_awaiting(this)) {
                m_result = m_awaited->get_result();
                return false;
            }
            return true;
        }
        T await_resume() {
            m_awaited->handle().destroy();
            return m_result.get_or_throw();
        }

        void set_results(task_result<T> result) noexcept override {
            m_result = std::move(result);
            m_enclosing->resume();
        }
    };


    template <class T>
    struct sync_awaitable : basic_awaitable<T> {
        task_result<T> m_result;
        promise<T>* m_awaited = nullptr;
        std::promise<task_result<T>> m_promise;
        std::future<task_result<T>> m_future = m_promise.get_future();

        sync_awaitable(promise<T>* awaited) : m_awaited(awaited) {
            m_awaited->resume_once();
            if ([[maybe_unused]] const bool ready = m_awaited->set_awaiting(this)) {
                m_result = m_awaited->get_result();
                await_resume();
            }
        }
        void set_results(task_result<T> result) noexcept override {
            m_result = std::move(result);
            await_resume();
        }
        void await_resume() noexcept {
            m_awaited->handle().destroy();
            m_promise.set_value(std::move(m_result));
        }
    };


} // namespace impl_eager_task


template <class T>
class [[nodiscard]] eager_task {
public:
    using promise_type = impl_eager_task::promise<T>;

    eager_task() = default;
    eager_task(const eager_task& rhs) = delete;
    eager_task& operator=(const eager_task& rhs) = delete;
    eager_task(eager_task&& rhs) noexcept : m_promise(std::exchange(rhs.m_promise, nullptr)) {}
    eager_task& operator=(eager_task&& rhs) noexcept {
        release_promise();
        m_promise = std::exchange(rhs.m_promise, nullptr);
        return *this;
    }
    eager_task(promise_type* promise) : m_promise(promise) {}
    ~eager_task() { release_promise(); }

    bool valid() const {
        return m_promise != nullptr;
    }

    void start() {
        assert(valid());
        m_promise->resume_once();
    }

    T get() {
        assert(valid());
        impl_eager_task::sync_awaitable<T> awaitable(std::exchange(m_promise, nullptr));
        return awaitable.m_future.get().get_or_throw();
    }

    auto operator co_await() {
        assert(valid());
        return impl_eager_task::awaitable<T>(std::exchange(m_promise, nullptr));
    }

    void set_scheduler(scheduler& scheduler) {
        if (m_promise) {
            m_promise->m_scheduler = &scheduler;
        }
    }

private:
    void release_promise() {
        if (valid()) {
            auto& pr = *std::exchange(m_promise, nullptr);
            if (!pr.m_resumed_once.test()) {
                pr.handle().destroy();
            }
            else if (pr.m_awaiting.exchange(pr.DONE) == pr.DONE) {
                pr.handle().destroy();
            }
        }
    }

private:
    promise_type* m_promise = nullptr;
};


} // namespace asyncpp