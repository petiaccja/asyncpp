#include <asyncpp/sleep.hpp>

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>


namespace asyncpp {

namespace impl_sleep {

    struct awaiter_priority {
        bool operator()(const awaitable* lhs, const awaitable* rhs) const noexcept {
            return lhs->get_time() > rhs->get_time();
        }
    };


    class sleep_scheduler {
    public:
        sleep_scheduler(sleep_scheduler&&) = delete;
        sleep_scheduler& operator=(sleep_scheduler&&) = delete;
        sleep_scheduler(const sleep_scheduler&) = delete;
        sleep_scheduler& operator=(const sleep_scheduler&) = delete;
        ~sleep_scheduler() {
            m_thread.request_stop();
            m_cvar.notify_all();
        }

        static sleep_scheduler& get() {
            static sleep_scheduler instance;
            return instance;
        }

        void enqueue(awaitable* awaiter) noexcept {
            {
                std::lock_guard lk(m_mtx);
                m_queue.push(awaiter);
            }
            m_cvar.notify_one();
        }

    private:
        void awake(std::stop_token token) {
            const auto stop_condition = [&] { return token.stop_requested() || !m_queue.empty(); };
            while (!token.stop_requested()) {
                std::unique_lock lk(m_mtx);
                if (m_queue.empty()) {
                    m_cvar.wait(lk, stop_condition);
                }
                else {
                    const auto next = m_queue.top();
                    m_cvar.wait_until(lk, next->get_time(), stop_condition);
                    if (next->get_time() <= clock_type::now()) {
                        m_queue.pop();
                        lk.unlock();
                        next->m_enclosing->resume();
                    }
                }
            }
        }

        sleep_scheduler() {
            m_thread = std::jthread([this](std::stop_token token) { awake(token); });
        }

    private:
        std::priority_queue<awaitable*, std::vector<awaitable*>, awaiter_priority> m_queue;
        std::mutex m_mtx;
        std::condition_variable m_cvar;
        std::jthread m_thread;
    };

    awaitable::awaitable(clock_type::time_point time) noexcept
        : m_time(time) {}

    bool awaitable::await_ready() const noexcept {
        return m_time < clock_type::now();
    }

    void awaitable::await_resume() const noexcept {
        return;
    }

    void awaitable::enqueue() noexcept {
        sleep_scheduler::get().enqueue(this);
    }

    auto awaitable::get_time() const noexcept -> clock_type::time_point {
        return m_time;
    }

} // namespace impl_sleep

} // namespace asyncpp