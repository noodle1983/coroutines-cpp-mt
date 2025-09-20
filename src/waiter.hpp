#pragma once

#include <coroutine>
#include <source_location>

#include "worker.hpp"
#include "log.hpp"

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
class TaskInnerWaiter {
public:
    template <typename T = ImplType, typename = std::enable_if_t<std::is_default_constructible_v<T>>>
    TaskInnerWaiter(const std::source_location& _loc) 
        : m_coroutine(nullptr), m_from_worker(nullptr), m_suspend_location(_loc)
    {}

    template<typename Arg0>
    TaskInnerWaiter(Arg0 _arg0, const std::source_location& _loc = std::source_location::current()) 
        : m_impl(this, _arg0), m_coroutine(nullptr), m_from_worker(nullptr), m_suspend_location(_loc) {}
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
        m_coroutine = _awaiting_coroutine;
        m_from_worker = Worker::GetCurrentWorker();
        m_impl.AwaitSuspend();
    }

    // NOLINTNEXTLINE
    ImplType::RetType await_resume() const noexcept {
        if constexpr (std::is_void_v<ImplType::RetType>) {

        } else {
			return m_impl.GetRetObj();
        }
    }

    void Resume() {
        if (m_from_worker == nullptr) {
            LOG_ERROR("no from worker to resume in, pre suspended from " << m_suspend_location.file_name() << ":" << m_suspend_location.line()); 
            return;
        }
        if (m_from_worker == Worker::GetCurrentWorker()) {
            ResumeInTheRightWorker();
        } else if (m_from_worker != NULL)
        {
            m_from_worker->AddJob(new nd::Job{[this]() { 
                ResumeInTheRightWorker();
            }});
        } 
    }

private:
    void ResumeInTheRightWorker() {
		if (m_coroutine) {
			auto coroutine = m_coroutine;
			m_coroutine = nullptr;
			coroutine.resume();
		}
    }


private:
    ImplType m_impl;
    std::coroutine_handle<> m_coroutine;
    nd::Worker* m_from_worker = nullptr;

    std::source_location m_suspend_location;
};
}  // namespace nd
