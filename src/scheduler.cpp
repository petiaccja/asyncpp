#include <async++/scheduler.hpp>


namespace asyncpp {


thread_pool::thread_pool(size_t num_threads) : m_state(std::make_shared<state>()) {
    for (size_t i = 0; i < num_threads; ++i) {
        m_threads.push_back(std::thread([state_ = m_state] { thread_function(state_); }));
    }
}


thread_pool::~thread_pool() {
    if (m_state) {
        m_state->is_running = false;
        {
            std::lock_guard lk(m_state->mtx);
            m_state->notifier.notify_all();
        }
        for (auto& th : m_threads) {
            th.join();
        }
    }
}


void thread_pool::schedule(impl::schedulable_promise& promise) {
    auto& next = m_state->next_in_schedule;
    impl::schedulable_promise* current = nullptr;
    do {
        promise.m_next_in_schedule = current;
    } while (!next.compare_exchange_weak(current, &promise, std::memory_order_relaxed, std::memory_order_relaxed));
    if (current == nullptr) {
        std::lock_guard lk(m_state->mtx);
        m_state->notifier.notify_all();
    }
}


void thread_pool::thread_function(std::shared_ptr<state> state_) {
    while (state_->is_running) {
        const auto next = pop(*state_);
        if (next) {
            next->handle().resume();
        }
        else {
            ptrdiff_t retries = 3000;
            while (state_->next_in_schedule.load(std::memory_order_relaxed) == nullptr && retries-- > 0) {
            }
            if (retries <= 0) {
                std::unique_lock lk(state_->mtx);
                state_->notifier.wait(lk, [&state_] {
                    return state_->next_in_schedule.load() != nullptr || !state_->is_running;
                });
            }
        }
    }
}


impl::schedulable_promise* thread_pool::pop(state& state_) {
    while (state_.pop_mtx.test_and_set(std::memory_order_acquire))
    {}
    impl::schedulable_promise* current = nullptr;
    impl::schedulable_promise* replacement;
    do {
        replacement = current ? current->m_next_in_schedule.load() : nullptr;
    } while (!state_.next_in_schedule.compare_exchange_weak(current, replacement, std::memory_order_relaxed, std::memory_order_relaxed));
    state_.pop_mtx.clear(std::memory_order_release);
    return current;
}


} // namespace asyncpp