#include <asyncpp/testing/suspension_point.hpp>
#include <asyncpp/thread_pool.hpp>


namespace asyncpp {


thread_pool::thread_pool(size_t num_threads)
    : m_workers(num_threads) {
    for (auto& w : m_workers) {
        w.thread = std::jthread([this, &w] {
            local = &w;
            execute(w, m_global_worklist, m_global_notification, m_global_mutex, m_terminate, m_num_waiting, m_workers);
        });
    }
}


thread_pool::~thread_pool() {
    std::lock_guard lk(m_global_mutex);
    m_terminate.test_and_set();
    m_global_notification.notify_all();
}


void thread_pool::schedule(schedulable_promise& promise) {
    schedule(promise, m_global_worklist, m_global_notification, m_global_mutex, m_num_waiting, local);
}


void thread_pool::schedule(schedulable_promise& item,
                           atomic_stack<schedulable_promise, &schedulable_promise::m_scheduler_next>& global_worklist,
                           std::condition_variable& global_notification,
                           std::mutex& global_mutex,
                           std::atomic_size_t& num_waiting,
                           worker* local) {
    if (local) {
        const auto prev_item = INTERLEAVED(local->worklist.push(&item));
        if (prev_item != nullptr) {
            if (num_waiting.load(std::memory_order_relaxed) > 0) {
                global_notification.notify_one();
            }
        }
    }
    else {
        std::unique_lock lk(global_mutex, std::defer_lock);
        INTERLEAVED_ACQUIRE(lk.lock());
        INTERLEAVED(global_worklist.push(&item));
        INTERLEAVED(global_notification.notify_one());
    }
}


schedulable_promise* thread_pool::steal(std::span<worker> workers) {
    for (auto& w : workers) {
        if (const auto item = INTERLEAVED(w.worklist.pop())) {
            return item;
        }
    }
    return nullptr;
}


void thread_pool::execute(worker& local,
                          atomic_stack<schedulable_promise, &schedulable_promise::m_scheduler_next>& global_worklist,
                          std::condition_variable& global_notification,
                          std::mutex& global_mutex,
                          std::atomic_flag& terminate,
                          std::atomic_size_t& num_waiting,
                          std::span<worker> workers) {
    do {
        if (const auto item = INTERLEAVED(local.worklist.pop())) {
            item->resume_now();
            continue;
        }
        else if (const auto item = INTERLEAVED(global_worklist.pop())) {
            local.worklist.push(item);
            continue;
        }
        else if (const auto item = steal(workers)) {
            local.worklist.push(item);
            continue;
        }
        else {
            std::unique_lock lk(global_mutex, std::defer_lock);
            INTERLEAVED_ACQUIRE(lk.lock());
            if (!INTERLEAVED(terminate.test()) && INTERLEAVED(global_worklist.empty())) {
                num_waiting.fetch_add(1, std::memory_order_relaxed);
                INTERLEAVED_ACQUIRE(global_notification.wait(lk));
                num_waiting.fetch_sub(1, std::memory_order_relaxed);
            }
        }
    } while (!INTERLEAVED(terminate.test()));
}


} // namespace asyncpp