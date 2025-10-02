#pragma once

#include <coroutine>
#include <source_location>

#include "worker.hpp"
#include "log.hpp"
#include "interfaces.hpp"

namespace nd {

// It records only one coroutine handle, so it can be used in only one coroutine at a time.
/*
 * class ImplType
 * {
 * public:
 *   using RetType = ...; // return type of await_resume
 *   bool AwaitReady(); // return true to skip suspension
 *   void AwaitSuspend(); // do something before suspension
 *   // call Resume() to resume the suspended coroutine
 *   RetType GetRetObj(); // return the object for await in the suspended coroutine/thread
 * };
 */
template<typename ImplType>
class TaskInnerWaiter : public IWaiter {
public:
    template <typename T = ImplType, typename = std::enable_if_t<std::is_default_constructible_v<T>>>
    TaskInnerWaiter(const std::source_location& _loc) 
        : m_task(nullptr), m_src_id(_loc), m_resume_key(0)
    {}

    template<typename Arg0>
    TaskInnerWaiter(Arg0 _arg0, const std::source_location& _loc = std::source_location::current()) 
        : m_impl(this, _arg0), m_src_id(_loc) {}
    //template<typename... Args>
    //TaskInnerWaiter(Args&&... _args, const std::source_location& _loc = std::source_location::current()) 
    //    : m_impl(this, std::forward<Args>(_args)...), m_coroutine(nullptr), m_from_worker(nullptr), m_suspend_location(_loc) {}
    virtual ~TaskInnerWaiter(){}

    // NOLINTNEXTLINE
    bool await_ready() noexcept {
        return m_impl.AwaitReady();
    }

    // NOLINTNEXTLINE
    void await_suspend(std::coroutine_handle<> _awaiting_coroutine) noexcept { 
        auto worker = Worker::GetCurrentWorker();
        m_task = worker->GetCurrentRunningTask();

        assert(m_task != nullptr);
        m_resume_key = m_task->GetResumeKey(this);

        m_impl.AwaitSuspend();
        worker->OnTaskSuspend(this);
    }

    // NOLINTNEXTLINE
    ImplType::RetType await_resume() const noexcept {
        if constexpr (std::is_void_v<ImplType::RetType>) {

        } else {
			return m_impl.GetRetObj();
        }
    }

    void Resume() { 
        assert(m_task != nullptr);
        m_task->Resume(this, m_resume_key);
        m_resume_key = 0;
    }

private:
    ITask* m_task;
    ImplType m_impl;

    nd::SrcId m_src_id;
    uint32_t m_resume_key;
};
}  // namespace nd
