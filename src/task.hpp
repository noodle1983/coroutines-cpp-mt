#ifndef TASK_H
#define TASK_H

#include "log.hpp"
#include "worker_manager.hpp"
#include "worker_types.hpp"
#include "interfaces.hpp"
#include "waiter.hpp"

#include <coroutine>
#include <iostream>
#include <list>
#include <tuple>
#include <type_traits>
#include <source_location>

#ifdef _MSC_VER
#define NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#else
#define NO_UNIQUE_ADDRESS [[no_unique_address]]
#endif

namespace nd {

template <typename ReturnType>
class Task;
template <typename ReturnType>
class TaskPromise;

/**
 * a task is a suspend-able function and also a waiter which suspends the upper task.
 * WrappedTaskWaiter make the task acts as a waiter. 
 * And it is copyable and is co-await in different tasks among different workers.
 * ref to waiter.hpp and GeneralWaiter
 */
template <typename ReturnType>
class WrappedTaskWaiter
{
public:
	using WaiterType = Task<ReturnType>;
	using RetType = ReturnType; // return type of await_resume
    WrappedTaskWaiter() : m_waiter(nullptr), m_is_done(false){ 
        LOG_TRACE(*this << " created.");
    }
	void SetWaiter(WaiterType* _waiter) { m_waiter = _waiter; }
    WrappedTaskWaiter(WrappedTaskWaiter&) = delete;
	virtual ~WrappedTaskWaiter(){}

	bool AwaitReady() { 
        MY_ASSERT(m_waiter->IsStarted(), "task must be started before awaited");
        return m_is_done;
    }
	void AwaitSuspend(){}
	void ResumeUpperTask() { m_waiter->Resume(); }
	void SetDone() {
		LOG_TRACE(*this << " done");
		m_is_done = true;
		ResumeUpperTask();
	}
	bool IsDone() { return m_is_done; }

    template <typename CheckType = ReturnType>
    void SaveResult(typename std::enable_if_t<!std::is_void_v<CheckType>, const ReturnType>& _value) {
        m_result = _value;
    };
    template <typename CheckType = ReturnType>
    void SaveResult(typename std::enable_if_t<!std::is_void_v<CheckType>, ReturnType>&& _value) {
        m_result = _value;
    };
    template <typename CheckType = ReturnType>
    typename std::enable_if_t<!std::is_void_v<CheckType>, const ReturnType>& GetRetObj() {
        return m_result;
    };

    void SaveException(std::exception_ptr _exception) { m_exception = _exception; }
    std::exception_ptr GetException() { return m_exception; }
    void CheckException() {
        if (m_exception) { std::rethrow_exception(m_exception); }
    }

    friend std::ostream& operator<<(std::ostream& os, const WrappedTaskWaiter<ReturnType>& obj) {
        os << "task-as-waiter-" << obj.m_id;
		return os;
	}

public:
    WaiterType* m_waiter;
    NO_UNIQUE_ADDRESS Maybe<!std::is_void_v<ReturnType>, ReturnType> m_result;
    std::atomic<bool> m_is_done;
    std::exception_ptr m_exception;
    ID<WrappedTaskWaiter<Empty>> m_id;
};

template <typename ReturnType>
class BaseTask : public ITask {
public:
    using promise_type = TaskPromise<ReturnType>;  // NOLINT
    using WaiterImplPtr = std::shared_ptr<WrappedTaskWaiter<ReturnType>>;

    BaseTask(WaiterImplPtr& _as_waiter_impl, std::coroutine_handle<promise_type> _handle) 
        : m_my_handle(_handle)
        , m_as_waiter_impl(_as_waiter_impl)
        , m_worker(nullptr) 
        , m_waiter(nullptr)
        , m_resume_key((uint32_t)time(nullptr))
    { LOG_TRACE(*this << " created"); }
    virtual ~BaseTask() {}

    virtual void AuthAndResume(IWaiter* waiter, uint32_t resume_key) override { 
        MY_ASSERT (waiter == m_waiter && resume_key == m_resume_key,
            "resume task from wrong waiter. suspended from [%p:%d], resumed from [%p:%d]", 
            m_waiter, m_resume_key, waiter, resume_key);
        m_waiter = nullptr;
        BaseResume(false);
    }

    virtual uint32_t GetResumeKey(IWaiter* waiter) override { 
        MY_ASSERT(m_waiter == nullptr, "waiter can't be null!");
        m_waiter = waiter;
        m_resume_key = m_resume_key * 1103515245 + 12345;
        return m_resume_key;
    }

    virtual IWaiter* GetWaiter() override { return m_waiter; }

    bool IsStarted() const { return m_worker != nullptr; }
    bool IsDone() const { return m_as_waiter_impl->IsDone(); }

protected:
    /****
     * The task is started in a new job which is a new running context.
     * So it has no impact on the current running task.
     */
    void BaseRunOnProcessor(int _worker_group_id = PreDefWorkerGroup::CurrentWorker, const SessionId _the_id = 0) {
        if (m_worker != nullptr) {
            // LOG_WARN("task can't run twice");
            return;
        }

        m_worker = g_worker_mgr->GetWorker(_worker_group_id, _the_id);
        BaseResume(true);
    }

    void BaseResume(bool _first_time){
        if (m_worker == nullptr) { return; }

        m_worker->AddJob(new nd::Job{[this, _first_time]() {
            if (!m_as_waiter_impl) { return; }
            if (_first_time) { 
				LOG_TRACE(*this << " run");
                Worker::GetCurrentWorker()->OnTaskStart(this);
            } else {
				LOG_TRACE(*this << " resume");
                Worker::GetCurrentWorker()->OnTaskRun(this); 
            }
            m_my_handle.resume();
        }});
    }

protected:
    std::coroutine_handle<promise_type> m_my_handle;
    WaiterImplPtr m_as_waiter_impl;
    nd::Worker* m_worker;
    IWaiter* m_waiter;
    uint32_t m_resume_key;
};

//-----------------------------------------
template <typename ReturnType>
class TaskPromise {
public:
    friend class Task<ReturnType>;
    using WaiterImplPtr = std::shared_ptr<WrappedTaskWaiter<ReturnType>>;

    TaskPromise() noexcept 
        : m_as_waiter_impl(new WrappedTaskWaiter<ReturnType>())
    { LOG_TRACE(*this << " created"); }
    virtual ~TaskPromise() { LOG_TRACE(*this << " destroyed"); }

    // NOLINTNEXTLINE
    auto initial_suspend() noexcept {
        LOG_TRACE(*this << " inital_suspend");
        return std::suspend_always{};
    }

    // NOLINTNEXTLINE
    auto final_suspend() noexcept {
        LOG_TRACE(*this << " final_suspend");
        return std::suspend_never{};
    }

    // NOLINTNEXTLINE
    Task<ReturnType> get_return_object() noexcept;

    // NOLINTNEXTLINE
    void return_value(const ReturnType& _value) noexcept {
        LOG_TRACE(*this << " return value&");
        m_as_waiter_impl->SaveResult(_value);
        m_as_waiter_impl->SetDone();
		Worker::GetCurrentWorker()->OnTaskEnd(); 
    }

    // NOLINTNEXTLINE
    void return_value(ReturnType&& _value) noexcept {
        LOG_TRACE(*this << " return value&&");
        m_as_waiter_impl->SaveResult(_value);
        m_as_waiter_impl->SetDone();
		Worker::GetCurrentWorker()->OnTaskEnd(); 
    }

    // NOLINTNEXTLINE
    void unhandled_exception() noexcept {
        LOG_TRACE(*this << " unhandled exception");
        m_as_waiter_impl->SaveException(std::current_exception());
        m_as_waiter_impl->SetDone();
		Worker::GetCurrentWorker()->OnTaskEnd(); 
    }

    friend std::ostream& operator<<(std::ostream& os, const TaskPromise<ReturnType>& obj) {
        os << "promise<R>-" << obj.m_id;
		return os;
	}

private:
    WaiterImplPtr m_as_waiter_impl;
    ID<TaskPromise<Empty>> m_id;
};

// It is illegal to have both return_value and return_void in a promise type, even if
// one of them is removed by SFINAE
// https://devblogs.microsoft.com/oldnewthing/20210330-00/?p=105019
template <>
class TaskPromise<void> {
public:
    friend class Task<void>;
    using WaiterImplPtr = std::shared_ptr<WrappedTaskWaiter<void>>;

    TaskPromise() noexcept 
        : m_as_waiter_impl(new WrappedTaskWaiter<void>())
    { LOG_TRACE(*this << " created"); }
    virtual ~TaskPromise() { LOG_TRACE(*this << " destroyed"); }

    // NOLINTNEXTLINE
    auto initial_suspend() noexcept {
        LOG_TRACE(*this << " inital_suspend");
        return std::suspend_always{};
    }

    // NOLINTNEXTLINE
    auto final_suspend() noexcept {
        LOG_TRACE(*this << " final_suspend");
        return std::suspend_never{};
    }

    // NOLINTNEXTLINE
    Task<void> get_return_object() noexcept;

    // NOLINTNEXTLINE
    void return_void() noexcept {
        LOG_TRACE(*this << " return void");
        m_as_waiter_impl->SetDone();
		Worker::GetCurrentWorker()->OnTaskEnd(); 
    }

    // NOLINTNEXTLINE
    void unhandled_exception() noexcept {
        LOG_TRACE(*this << " unhandled exception");
        m_as_waiter_impl->SaveException(std::current_exception());
        m_as_waiter_impl->SetDone();
		Worker::GetCurrentWorker()->OnTaskEnd(); 
    }

    friend std::ostream& operator<<(std::ostream& os, const TaskPromise<void>& obj) {
        os << "promise<>-" << obj.m_id;
		return os;
	}

private:
    WaiterImplPtr m_as_waiter_impl;
    ID<TaskPromise<Empty>> m_id;
};


template <typename ReturnType = void>
class Task : public BaseTask<ReturnType>, public GeneralWaiter< WrappedTaskWaiter<ReturnType> >{
public:
    using promise_type = TaskPromise<ReturnType>;  // NOLINT
    using WaiterImplPtr = std::shared_ptr<WrappedTaskWaiter<ReturnType>>;
    using ParentTask = BaseTask<ReturnType>;
    using ParentWaiter = GeneralWaiter< WrappedTaskWaiter<ReturnType> >;

    Task(WaiterImplPtr& _as_waiter_impl, std::coroutine_handle<promise_type> _handle) 
        : ParentTask(_as_waiter_impl, _handle)
        , ParentWaiter(_as_waiter_impl, std::source_location::current())
    {
        _as_waiter_impl->SetWaiter(this);
    }
    virtual ~Task() { LOG_TRACE("task-" << ParentTask::m_id << " destroyed"); }
    virtual std::ostream& GetWaiterDesc(std::ostream& os) const override{ return os << (ParentTask&)*this; }

    Task& RunOnProcessor(int _worker_group_id = PreDefWorkerGroup::CurrentWorker, const SessionId _the_id = 0, const char* _stat_name = nullptr, const std::source_location& _loc = std::source_location::current()) {
        ITask::SetStatInfo(_stat_name, _loc);
        ParentTask::BaseRunOnProcessor(_worker_group_id, _the_id);
        return *this;
    }

    // wait for the task to complete in main thread
    void WaitInMain() {
        while (!ParentTask::IsDone()) { Worker::GetCurrentWorker()->Step(); }
    }

private:
};

template <typename ReturnType>
Task<ReturnType> TaskPromise<ReturnType>::get_return_object() noexcept {
    LOG_TRACE(*this << " get_return_object");
    return Task<ReturnType>(m_as_waiter_impl, std::coroutine_handle<TaskPromise>::from_promise(*this));
}

inline Task<void> TaskPromise<void>::get_return_object() noexcept {
    LOG_TRACE(*this << " get_return_object");
    return Task<void>(m_as_waiter_impl, std::coroutine_handle<TaskPromise>::from_promise(*this));
}
}  // namespace nd

#endif /* TASK_H */
