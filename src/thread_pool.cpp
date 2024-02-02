#include <asyncpp/thread_pool.hpp>

#include <algorithm>
#include <format>


namespace asyncpp {


thread_local std::shared_ptr<thread_pool::worker> thread_pool::local_worker = nullptr;
thread_local thread_pool* thread_pool::local_owner = nullptr;


thread_pool::thread_pool(size_t num_threads) {
    std::ranges::generate_n(std::back_inserter(m_workers), num_threads, [this] {
        return std::make_shared<worker>();
    });
    std::ranges::transform(m_workers, std::back_inserter(m_os_threads), [this](const auto& w) {
        return std::jthread([this, &w] { worker_function(w); });
    });
}


thread_pool::~thread_pool() noexcept {
    std::ranges::for_each(m_workers, [](auto& w) {
        w->stop();
        w->wake();
    });
}


void thread_pool::schedule(schedulable_promise& promise) noexcept {
    if (m_free_workers.empty() && local_owner == this) {
        local_worker->add_work_item(promise);
    }
    else if (const auto free_worker = m_free_workers.pop()) {
        free_worker->add_work_item(promise);
        free_worker->wake();
    }
    else {
        const auto& worker = m_workers[0];
        const bool has_work = worker->has_work();
        worker->add_work_item(promise);
        if (has_work) {
            worker->wake();
        }
    }
}


void thread_pool::worker_function(std::shared_ptr<worker> w) noexcept {
    local_worker = w;
    local_owner = this;

    schedulable_promise* promise;
    do {
        promise = w->get_work_item();

        for (auto it = m_workers.begin(); it != m_workers.end() && !promise; ++it) {
            promise = (*it)->get_work_item();
        }

        if (promise) {
            promise->handle().resume();
        }
        else {
            m_free_workers.push(w.get());
            w->wait();
        }
    } while (promise || !w->is_stopped());
}


void thread_pool::worker::add_work_item(schedulable_promise& promise) {
    m_work_items.push(&promise);
}


schedulable_promise* thread_pool::worker::get_work_item() {
    return m_work_items.pop();
}


bool thread_pool::worker::has_work() const {
    return !m_work_items.empty();
}


bool thread_pool::worker::is_stopped() const {
    return m_terminated.test();
}


void thread_pool::worker::stop() {
    m_terminated.test_and_set();
}


void thread_pool::worker::wake() const {
    std::lock_guard lk(m_wake_mutex);
    m_wake_cv.notify_one();
}


void thread_pool::worker::wait() const {
    std::unique_lock lk(m_wake_mutex);
    m_wake_cv.wait(lk, [this] { return has_work() || is_stopped(); });
}


} // namespace asyncpp