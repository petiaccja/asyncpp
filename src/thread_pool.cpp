#include <asyncpp/testing/suspension_point.hpp>
#include <asyncpp/thread_pool.hpp>


namespace asyncpp {


thread_pool::worker::worker()
    : m_sema(0) {}


thread_pool::worker::~worker() {
    cancel();
}


void thread_pool::worker::insert(schedulable_promise& promise) {
    std::unique_lock lk(m_spinlock, std::defer_lock);
    INTERLEAVED_ACQUIRE(lk.lock());
    const auto previous = m_promises.push_back(&promise);
    const auto blocked = m_blocked.test(std::memory_order_relaxed);
    INTERLEAVED(lk.unlock());

    if (!previous && blocked) {
        INTERLEAVED(m_sema.release());
    }
}


schedulable_promise* thread_pool::worker::steal_from_this() {
    std::unique_lock lk(m_spinlock, std::defer_lock);
    INTERLEAVED(lk.lock());
    return m_promises.pop_front();
}


schedulable_promise* thread_pool::worker::try_get_promise(pack& pack, size_t& stealing_attempt, bool& exit_loop) {
    std::unique_lock lk(m_spinlock, std::defer_lock);
    INTERLEAVED_ACQUIRE(lk.lock());
    const auto promise = m_promises.front();
    if (promise) {
        m_promises.pop_front();
        return promise;
    }

    if (stealing_attempt > 0) {
        INTERLEAVED(lk.unlock());
        const auto stolen = steal_from_other(pack, stealing_attempt);
        stealing_attempt = stolen ? pack.workers.size() : stealing_attempt - 1;
        return stolen;
    }


    if (INTERLEAVED(m_cancelled.test(std::memory_order_relaxed))) {
        exit_loop = true;
    }
    else {
        INTERLEAVED(m_blocked.test_and_set(std::memory_order_relaxed));
        pack.blocked.push(this);
        pack.num_blocked.fetch_add(1, std::memory_order_relaxed);
        INTERLEAVED(lk.unlock());
        INTERLEAVED_ACQUIRE(m_sema.acquire());
        INTERLEAVED(m_blocked.clear());
        stealing_attempt = pack.workers.size();
    }
    return nullptr;
}


schedulable_promise* thread_pool::worker::steal_from_other(pack& pack, size_t& stealing_attempt) const {
    const size_t pack_size = pack.workers.size();
    const size_t my_index = this - pack.workers.data();
    const size_t victim_index = (my_index + stealing_attempt) % pack_size;
    return pack.workers[victim_index].steal_from_this();
}


void thread_pool::worker::start(pack& pack) {
    m_thread = std::jthread([this, &pack] {
        run(pack);
    });
}


void thread_pool::worker::cancel() {
    std::unique_lock lk(m_spinlock, std::defer_lock);
    INTERLEAVED_ACQUIRE(lk.lock());
    INTERLEAVED(m_cancelled.test_and_set(std::memory_order_relaxed));
    const auto blocked = INTERLEAVED(m_blocked.test(std::memory_order_relaxed));
    lk.unlock();
    if (blocked) {
        INTERLEAVED(m_sema.release());
    }
}


void thread_pool::worker::run(pack& pack) {
    m_local = this;
    size_t stealing_attempt = pack.workers.size();
    bool exit_loop = false;
    while (!exit_loop) {
        const auto promise = try_get_promise(pack, stealing_attempt, exit_loop);
        if (promise) {
            promise->resume_now();
        }
    }
}


thread_pool::thread_pool(size_t num_threads)
    : m_pack(std::vector<worker>(num_threads)), m_next_in_schedule(0) {
    for (auto& worker : m_pack.workers) {
        worker.start(m_pack);
    }
}


void thread_pool::schedule(schedulable_promise& promise) {
    size_t num_blocked = m_pack.num_blocked.load(std::memory_order_relaxed);
    const auto blocked = num_blocked > 0 ? m_pack.blocked.pop() : nullptr;
    if (blocked) {
        blocked->insert(promise);
        m_pack.num_blocked.fetch_sub(1, std::memory_order_relaxed);
    }
    else if (m_local) {
        m_local->insert(promise);
    }
    else {
        const auto selected = m_next_in_schedule.fetch_add(1, std::memory_order_relaxed) % m_pack.workers.size();
        m_pack.workers[selected].insert(promise);
    }
}


} // namespace asyncpp