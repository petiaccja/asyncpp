#include <async++/scheduler.hpp>

#include <algorithm>
#include <format>


namespace asyncpp {

thread_local thread_pool::worker_state thread_pool::m_worker_state;


thread_pool::thread_pool(size_t num_threads) : m_worker_threads(num_threads), m_worker_states(num_threads) {
    std::ranges::for_each(m_worker_states, [](auto& state) { state.store(nullptr); });
    std::ranges::generate(m_worker_threads, [this] { return std::thread([this] { worker_function(); }); });
    sync_worker_init();
}


thread_pool::~thread_pool() noexcept {
    m_finished.test_and_set();
    std::ranges::for_each(m_worker_states, [](auto& state) { state.load()->m_notifier.notify_one(); });
    std::ranges::for_each(m_worker_threads, [](auto& worker) { worker.join(); });
}


void thread_pool::schedule(impl::schedulable_promise& promise) noexcept {
    do {
        auto worker = m_free_workers.pop();
        if (worker != nullptr) {
            // Nobody else can touch worker's list in the meantime.
            worker->m_work_items.push(&promise);
            std::lock_guard lk(worker->m_notification_mutex);
            worker->m_notifier.notify_one();
            break;
        }
        else if (m_worker_state.m_owner == this) {
            m_worker_state.m_work_items.push(&promise);
            break;
        }
        else {
            worker = m_worker_states[0];
            const bool was_empty = worker->m_work_items.push(&promise) == nullptr;
            // An empty work list means the worker finished its work items.
            if (was_empty) {
                // Let's move it to the busy list and wake it up.
                std::lock_guard lk(worker->m_notification_mutex);
                worker->m_notifier.notify_one();
            }
            break;
        }
    } while (true);
}


void thread_pool::worker_function() noexcept {
    const auto index = m_worker_index.fetch_add(1);
    m_worker_state.m_owner = this;
    m_worker_states[index] = &m_worker_state; // Synchronize: this must be the last step of initialization. See ctor.
    sync_worker_init();

    impl::schedulable_promise* work_item = nullptr;
    do {
        // Try to get a work item from the local queue.
        work_item = m_worker_state.m_work_items.pop();
        // If failed, try to steal a work item from another worker.
        if (!work_item) {
            for (auto& victim_worker : m_worker_states) {
                work_item = victim_worker.load(std::memory_order_relaxed)->m_work_items.pop();
                if (work_item) {
                    break;
                }
            }
        }
        // If failed, try to sleep until there is a work item.
        if (!work_item) {
            m_free_workers.push(&m_worker_state);
            std::unique_lock lk(m_worker_state.m_notification_mutex);
            m_worker_state.m_notifier.wait(lk, [this, &work_item] {
                work_item = m_worker_state.m_work_items.pop();
                return work_item || m_finished.test();
            });
        }
        // Process work item if any.
        if (work_item) {
            work_item->handle().resume();
        }
    } while (work_item || !m_finished.test());
}


void thread_pool::sync_worker_init() noexcept {
    // Wait until all threads initialized themselves.
    bool success = false;
    do {
        success = std::ranges::all_of(m_worker_states, [](auto& state) {
            return state.load(std::memory_order_relaxed) != nullptr;
        });
    } while (!success);
}


} // namespace asyncpp