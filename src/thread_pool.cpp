#include <asyncpp/testing/suspension_point.hpp>
#include <asyncpp/thread_pool.hpp>


namespace asyncpp {


thread_pool::thread_pool(size_t num_threads)
    : m_workers(num_threads) {
    for (auto& w : m_workers) {
        w.thread = std::jthread([this, &w] {
            local = &w;
            execute(w, m_global_worklist, m_global_notification, m_terminate, m_workers);
        });
    }
}


thread_pool::~thread_pool() {
    m_terminate.test_and_set();
    m_global_notification.notify_all();
}


void thread_pool::schedule(schedulable_promise& promise) {
    schedule(promise, m_global_worklist, m_global_notification, local);
}


void thread_pool::schedule(schedulable_promise& item,
                           atomic_stack<schedulable_promise, &schedulable_promise::m_scheduler_next>& global_worklist,
                           std::condition_variable& global_notification,
                           worker* local) {
    if (local) {
        const auto prev_item = INTERLEAVED(local->worklist.push(&item));
        if (prev_item != nullptr) {
            INTERLEAVED(global_notification.notify_one()); // Notify one thread to potentially steal item.
        }
    }
    else {
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
                          std::atomic_flag& terminate,
                          std::span<worker> workers) {
    std::mutex mtx;
    do {
        const auto item = INTERLEAVED(local.worklist.pop());
        if (item != nullptr) {
            item->handle().resume();
        }
        else {
            std::unique_lock lk(mtx);
            global_notification.wait(lk, [&] {
                const auto global = INTERLEAVED(global_worklist.pop());
                if (global) {
                    local.worklist.push(global);
                    return INTERLEAVED_ACQUIRE(true);
                }
                const auto stolen = steal(workers);
                if (stolen) {
                    local.worklist.push(stolen);
                    return INTERLEAVED_ACQUIRE(true);
                }
                return INTERLEAVED_ACQUIRE(terminate.test());
            });
        }
    } while (!INTERLEAVED(local.worklist.empty()) || !INTERLEAVED(global_worklist.empty())|| !INTERLEAVED(terminate.test()));
}


} // namespace asyncpp