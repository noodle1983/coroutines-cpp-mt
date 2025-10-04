#pragma once

#include "worker.hpp"
#include "log.hpp"
#include "interfaces.hpp"

#include <coroutine>
#include <source_location>
#include <type_traits>
#include <map>

namespace nd {


template<typename ImplType>
class TaskInnerWaiter;
template<typename ImplType>
class GeneralWaiter;

class ImplTypeExample
{
public:
	using WaiterType = TaskInnerWaiter<ImplTypeExample>;
	// or using WaiterType = GeneralWaiter<ImplTypeExample>;
	using RetType = void; // return type of await_resume

	bool AwaitReady(); // return true to skip suspension
	void AwaitSuspend(); // do something before suspension
	// call Resume() to resume the suspended coroutine
	RetType GetRetObj(); // return the object for await in the suspended coroutine/thread
};


template<typename Impl, typename... Args>
struct is_constructible_with_waiter : std::is_constructible<Impl, TaskInnerWaiter<Impl>*, Args...> {};

// TaskInnerWaiter records only one coroutine handle, so it can be used in only one coroutine at a time.
template<typename ImplType>
class TaskInnerWaiter : public IWaiter {
public:
    template <typename T = ImplType, 
        typename = std::enable_if_t<
                 is_constructible_with_waiter<ImplType>::value>>
    TaskInnerWaiter(const std::source_location& _loc) 
        : m_task(nullptr), m_impl(this), m_src_id(_loc), m_resume_key(0)
    {}

    template<typename Arg0,
             typename = std::enable_if_t<
                 is_constructible_with_waiter<ImplType, Arg0>::value>>
    TaskInnerWaiter(Arg0 _arg0, const std::source_location& _loc = std::source_location::current()) 
        : m_impl(this, _arg0), 
          m_src_id(_loc), 
          m_resume_key(0) 
    {}

    template<typename... Args,
             typename = std::enable_if_t<
                 is_constructible_with_waiter<ImplType, Args...>::value>>
    TaskInnerWaiter(Args&&... _args, 
                   const std::source_location& _loc = std::source_location::current()) 
        : m_impl(this, std::forward<Args>(_args)...), 
          m_src_id(_loc), 
          m_resume_key(0) 
    {}
    virtual ~TaskInnerWaiter(){}

    // NOLINTNEXTLINE
    bool await_ready() noexcept {
        return m_impl.AwaitReady();
    }

    // NOLINTNEXTLINE
    void await_suspend(std::coroutine_handle<> _awaiting_coroutine) noexcept { 
        auto worker = Worker::GetCurrentWorker();
        m_task = worker->GetCurrentRunningTask();

        MY_ASSERT(m_task != nullptr, "must await in a task!");
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
        MY_ASSERT(m_task != nullptr, "not await yet!");
        m_task->Resume(this, m_resume_key);
        m_resume_key = 0;
    }

private:
    ITask* m_task;
    ImplType m_impl;

    nd::SrcId m_src_id;
    uint32_t m_resume_key;
};

template<typename ImplType>
class GeneralWaiter : public IWaiter {
public:
    template <typename T = ImplType, 
        typename = std::enable_if_t<
                 is_constructible_with_waiter<ImplType>::value>>
    GeneralWaiter(const std::source_location& _loc) 
        : m_impl(std::make_shared(new ImplType(this)))
        , m_src_id(_loc)
    {}

    template<typename Arg0,
             typename = std::enable_if_t<
                 is_constructible_with_waiter<ImplType, Arg0>::value>>
    GeneralWaiter(Arg0 _arg0, const std::source_location& _loc = std::source_location::current()) 
        : m_impl(std::make_shared(new ImplType(this, _arg0)))
        , m_src_id(_loc)
    {}

    template<typename... Args,
             typename = std::enable_if_t<
                 is_constructible_with_waiter<ImplType, Args...>::value>>
    GeneralWaiter(Args&&... _args, 
                   const std::source_location& _loc = std::source_location::current()) 
        : m_impl(std::make_shared(this, std::forward<Args>(_args)...))
        , m_src_id(_loc)
    {}
    virtual ~GeneralWaiter(){}

    // NOLINTNEXTLINE
    bool await_ready() noexcept {
        return m_impl->AwaitReady();
    }

    // NOLINTNEXTLINE
    void await_suspend(std::coroutine_handle<> _awaiting_coroutine) noexcept { 
        auto worker = Worker::GetCurrentWorker();
        auto task = worker->GetCurrentRunningTask();

        MY_ASSERT(task != nullptr, "must await in a task!");

        auto it = m_tasks_info.find(task);

        MY_ASSERT(it == m_tasks_info.end(), "task is already been suspended!");
        m_tasks_info[task] = task->GetResumeKey(this);

        m_impl.AwaitSuspend();
        worker->OnTaskSuspend(this);
    }

    // NOLINTNEXTLINE
    ImplType::RetType await_resume() const noexcept {
        if constexpr (std::is_void_v<ImplType::RetType>) {

        } else {
			return m_impl->GetRetObj();
        }
    }

    void Resume() { 
        MY_ASSERT(!m_tasks_info.empty(), "not await yet!");
        for (auto it = m_tasks_info.begin(); it != m_tasks_info.end(); it++){
            auto task = it->key();
            auto resume_key = it->value();
            task->Resume(this, resume_key);
        }
        m_tasks_info.clear();
    }

private:
    std::map<ITask*, uint32_t> m_tasks_info;
    std::shared_ptr<ImplType> m_impl;

    nd::SrcId m_src_id;
};
}  // namespace nd
