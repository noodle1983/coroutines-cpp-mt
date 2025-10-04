#pragma once

#include "worker.hpp"
#include "log.hpp"
#include "interfaces.hpp"

#include <coroutine>
#include <source_location>
#include <type_traits>
#include <map>
#include <memory>

namespace nd {


template<typename ImplType>
class TaskInnerWaiter;
template<typename ImplType>
class GeneralWaiter;

class WrappedWaiterImplTypeExample
{
public:
	using WaiterType = TaskInnerWaiter<WrappedWaiterImplTypeExample>;
	// or using WaiterType = GeneralWaiter<ImplTypeExample>;
	using RetType = void; // return type of await_resume
    //WrappedWaiterImplTypeExample(WaiterType* _waiter, ...) : m_waiter(_waiter){ } 
	//virtual ~WrappedWaiterImplTypeExample(){}

	bool AwaitReady();   // as IsDone, return true to skip suspension
	void AwaitSuspend(); // do something before suspension
	// call m_waiter->Resume() to resume the suspended coroutine
	RetType GetRetObj(); // return the object for await in the suspended coroutine/thread
private:
	WaiterType* m_waiter;
};


template<typename Impl, typename... Args>
struct is_constructible_with_task_inner_waiter : std::is_constructible<Impl, TaskInnerWaiter<Impl>*, Args...> {};

// TaskInnerWaiter records only one coroutine handle, 
// so it can be used in only one coroutine at a time and thus no mutex.
template<typename ImplType>
class TaskInnerWaiter : public IWaiter {
public:
    template <typename T = ImplType, 
        typename = std::enable_if_t<
                 is_constructible_with_task_inner_waiter<ImplType>::value>>
    TaskInnerWaiter(const std::source_location& _loc) 
        : m_task(nullptr), m_impl(this), m_src_id(_loc), m_resume_key(0)
    {}

    template<typename Arg0,
             typename = std::enable_if_t<
                 is_constructible_with_task_inner_waiter<ImplType, Arg0>::value>>
    TaskInnerWaiter(Arg0 _arg0, const std::source_location& _loc = std::source_location::current()) 
        : m_impl(this, _arg0), 
          m_src_id(_loc), 
          m_resume_key(0) 
    {}

    template<typename... Args,
             typename = std::enable_if_t<
                 is_constructible_with_task_inner_waiter<ImplType, Args...>::value>>
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

    friend std::ostream& operator<<(std::ostream& os, const TaskInnerWaiter<ImplType>& obj) {
        char buff[128] = {0};
        obj.m_src_id.Get(buff, sizeof(buff));
        os << "InnerWaiter[" << buff << ']';
		return os;
	}

private:
    ITask* m_task;
    ImplType m_impl;

    nd::SrcId m_src_id;
    uint32_t m_resume_key;
};

template<typename ImplType>
class GeneralWaiter;
template<typename Impl, typename... Args>
struct is_constructible_with_general_waiter : std::is_constructible<Impl, GeneralWaiter<Impl>*, Args...> {};

template<typename ImplType>
class GeneralWaiter : public IWaiter {
public:
    GeneralWaiter(std::shared_ptr<ImplType>& _impl, const std::source_location& _loc) 
        : m_impl(_impl), m_src_id(_loc)
    {}

    template <typename T = ImplType, 
        typename = std::enable_if_t<
                 is_constructible_with_general_waiter<ImplType>::value>>
    GeneralWaiter(const std::source_location& _loc) 
        : m_impl(std::make_shared<ImplType>(this))
        , m_src_id(_loc)
    {}

    template<typename Arg0,
             typename = std::enable_if_t<
                 is_constructible_with_general_waiter<ImplType, Arg0>::value>>
    GeneralWaiter(Arg0 _arg0, const std::source_location& _loc = std::source_location::current()) 
        : m_impl(std::make_shared<ImplType>(this, _arg0))
        , m_src_id(_loc)
    {}

    template<typename... Args,
             typename = std::enable_if_t<
                 is_constructible_with_general_waiter<ImplType, Args...>::value>>
    GeneralWaiter(Args&&... _args, 
                   const std::source_location& _loc = std::source_location::current()) 
        : m_impl(std::make_shared<ImplType>(this, std::forward<Args>(_args)...))
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

        {
            std::lock_guard<std::mutex> lock(m_tasks_mutex);
            auto it = m_tasks_info.find(task);

            MY_ASSERT(it == m_tasks_info.end(), "task is already been suspended!");
            m_tasks_info[task] = task->GetResumeKey(this);
        }

        m_impl->AwaitSuspend();
        worker->OnTaskSuspend(this);

        // incase it is done in another thread
        if (m_impl->AwaitReady()) { Resume(); }
    }

    // can't catch exception in this version
    // NOLINTNEXTLINE
    //ImplType::RetType await_resume() const noexcept {
    //    if constexpr (std::is_void_v<ImplType::RetType>) {
	//		m_impl->CheckException();
    //    } else {
	//		m_impl->CheckException();
	//		return m_impl->GetRetObj();
    //    }
    //}
	template <typename CheckType = ImplType::RetType>  // NOLINTNEXTLINE
    typename std::enable_if_t<std::is_void_v<CheckType>, void> await_resume() const {
        m_impl->CheckException();
    }

	template <typename CheckType = ImplType::RetType>  // NOLINTNEXTLINE
    typename std::enable_if_t<!std::is_void_v<CheckType>, const CheckType>& await_resume() const {
        m_impl->CheckException();
        return m_impl->GetRetObj();
    }

    void Resume() { 
		std::lock_guard<std::mutex> lock(m_tasks_mutex);
        if (m_tasks_info.empty()) { return; } // if no task is awaiting

        for (auto it = m_tasks_info.begin(); it != m_tasks_info.end(); it++){
            auto task = it->first;
            auto resume_key = it->second;
            task->Resume(this, resume_key);
        }
        m_tasks_info.clear();
    }

    friend std::ostream& operator<<(std::ostream& os, const TaskInnerWaiter<ImplType>& obj) {
        char buff[128] = {0};
        obj.m_src_id.Get(buff, sizeof(buff));
        os << "GenWaiter[" << buff << ']';
		return os;
	}

private:
    std::map<ITask*, uint32_t> m_tasks_info;
    std::mutex m_tasks_mutex;
    std::shared_ptr<ImplType> m_impl;

    nd::SrcId m_src_id;
};
}  // namespace nd
