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
        schedulable_promise* m_scheduler_next = nullptr;
        scheduler* m_scheduler = nullptr;
    };


} // namespace impl
} // namespace asyncpp