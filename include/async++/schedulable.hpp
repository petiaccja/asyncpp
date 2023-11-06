#pragma once

#include <atomic>
#include <coroutine>


namespace asyncpp {


class scheduler;


namespace impl {


    struct resumable_promise {
        virtual ~resumable_promise() = default;
        virtual void resume() = 0;
    };


    struct schedulable_promise {
        virtual ~schedulable_promise() = default;
        virtual std::coroutine_handle<> handle() = 0;
        std::atomic<schedulable_promise*> m_next_in_schedule = nullptr;
        scheduler* m_scheduler = nullptr;
    };


} // namespace impl
} // namespace asyncpp