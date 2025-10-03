#include "log.hpp"
#include "worker_manager.hpp"
#include "worker_types.hpp"
#include "interfaces.hpp"

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

//-----------------------------------------
// Control the lifetime of the task and coroutine
// If the return task is not holded in any where, it lives until the coroutine exits.
// Otherwise, it lives until the last hold task is done.
//-----------------------------------------
template <typename ReturnType = void>
class CoroutineController {
public:
    using WaitingTask = std::tuple<BaseTask<ReturnType>*, Worker*>;

    CoroutineController(std::coroutine_handle<> _coroutine) : m_coroutine(_coroutine) {
        LOG_TRACE("controller-" << m_id << " created");
    }
    virtual ~CoroutineController() {
        if (m_coroutine) {
            m_coroutine.destroy();
            m_coroutine = nullptr;
        }
        LOG_TRACE("controller-" << m_id << " destroyed");
    }

    // please make sure the handle is valid before use it
    // it is only used in resume
    std::coroutine_handle<> Handle() const { return m_coroutine; }

    void AddWaitingTask(BaseTask<ReturnType>* _task, Worker* _worker);
    void OnCoroutineReturn();
    void OnCoroutineDone();

    bool IsDone() {
        std::lock_guard<std::mutex> lock(m_waiting_tasks_mutex);
        return m_coroutine == nullptr;
    }

    template <typename CheckType = ReturnType>
    void SaveResult(typename std::enable_if_t<!std::is_void_v<CheckType>, const ReturnType>& _value) {
        m_result = _value;
    };
    template <typename CheckType = ReturnType>
    void SaveResult(typename std::enable_if_t<!std::is_void_v<CheckType>, ReturnType>&& _value) {
        m_result = _value;
    };
    template <typename CheckType = ReturnType>
    typename std::enable_if_t<!std::is_void_v<CheckType>, const ReturnType>& GetResult() {
        return m_result;
    };

    void SaveException(std::exception_ptr _exception) { m_exception = _exception; }
    std::exception_ptr GetException() { return m_exception; }
    void CheckException() {
        if (m_exception) { std::rethrow_exception(m_exception); }
    }

private:
    ID<CoroutineController<Empty>> m_id;

    std::list<WaitingTask> m_waiting_tasks;
    std::mutex m_waiting_tasks_mutex;

    std::coroutine_handle<> m_coroutine;

    NO_UNIQUE_ADDRESS Maybe<!std::is_void_v<ReturnType>, ReturnType> m_result;
    std::exception_ptr m_exception;
};

static_assert(sizeof(CoroutineController<void>) != sizeof(CoroutineController<char>));


template <typename ReturnType = void>
class BaseTask : public ITask {
public:
    friend class CoroutineController<ReturnType>;
    using CorotineControllerSharedPtr = std::shared_ptr<CoroutineController<ReturnType>>;

    BaseTask(CorotineControllerSharedPtr& _controller) 
        : m_controller(_controller)
        , m_worker(nullptr) 
        , m_waiter(nullptr)
        , m_resume_key((uint32_t)time(nullptr))
    {}
    virtual ~BaseTask() {}

    /****
     * The task is started in a new job which is a new running context.
     * So it has no impact on the current running task.
     */
    void BaseRunOnProcessor(int _worker_group_id = PreDefWorkerGroup::Current, const SessionId _the_id = 0) {
        if (m_worker != nullptr) {
            // LOG_WARN("task can't run twice");
            return;
        }

        m_worker = g_worker_mgr->GetWorker(_worker_group_id, _the_id);
        BaseResume(true);
    }

    virtual void Resume(IWaiter* waiter, uint32_t resume_key) override { 
        if (waiter != m_waiter || resume_key != m_resume_key)
        { 
            FLOG_FATAL("resume task from wrong waiter. suspended from [%p:%d], resumed from [%p:%d]", m_waiter, m_resume_key, waiter, resume_key);
            abort();
            return;
        }
        m_waiter = nullptr;
        BaseResume(false);
    }

    virtual uint32_t GetResumeKey(IWaiter* waiter) override { 
        MY_ASSERT(m_waiter == nullptr, "waiter can't be null!");
        m_waiter = waiter;
        m_resume_key = m_resume_key * 1103515245 + 12345;
        return m_resume_key;
    }

    void WaitReturn(Worker* _worker) { 
        m_controller->AddWaitingTask(this, _worker); 
    }

    virtual void OnCoroutineReturn() {
    }

    bool IsDone() const { return m_controller->IsDone(); }

protected:

    void BaseResume(bool _first_time){
        if (m_worker == nullptr) { return; }

        m_worker->AddJob(new nd::Job{[this, _first_time]() {
            if (!m_controller) { return; }
            if (_first_time) { 
				LOG_TRACE("task-" << m_id << " run in worker");
                Worker::GetCurrentWorker()->OnTaskStart(this);
            } else {
				LOG_TRACE("task-" << m_id << " resume in worker");
                Worker::GetCurrentWorker()->OnTaskRun(this); 
            }
            m_controller->Handle().resume();
        }});
    }

protected:
    CorotineControllerSharedPtr m_controller;
    nd::Worker* m_worker;
    IWaiter* m_waiter;
    uint32_t m_resume_key;
};

//-----------------------------------------
template <typename ReturnType>
class TaskPromise {
public:
    friend class Task<ReturnType>;
    using CorotineControllerSharedPtr = std::shared_ptr<CoroutineController<ReturnType>>;

    TaskPromise() noexcept {
        LOG_TRACE("promise-" << m_id << " created");

        m_controller =
            std::make_shared<CoroutineController<ReturnType>>(std::coroutine_handle<TaskPromise>::from_promise(*this));
    }
    virtual ~TaskPromise() { LOG_TRACE("promise-" << m_id << " destroyed"); }

    // NOLINTNEXTLINE
    auto initial_suspend() noexcept {
        LOG_TRACE("promise-" << m_id << " inital_suspend");
        return std::suspend_always{};
    }

    // NOLINTNEXTLINE
    auto final_suspend() noexcept {
        LOG_TRACE("promise-" << m_id << " final_suspend");
        m_controller->OnCoroutineDone();
        return std::suspend_never{};
    }

    // NOLINTNEXTLINE
    Task<ReturnType> get_return_object() noexcept;

    // NOLINTNEXTLINE
    void return_value(const ReturnType& _value) noexcept {
        LOG_TRACE("promise-" << m_id << " return value&");
        m_controller->SaveResult(_value);
        m_controller->OnCoroutineReturn();
		Worker::GetCurrentWorker()->OnTaskEnd(); 
    }

    // NOLINTNEXTLINE
    void return_value(ReturnType&& _value) noexcept {
        LOG_TRACE("promise-" << m_id << " return value&&");
        m_controller->SaveResult(_value);
        m_controller->OnCoroutineReturn();
		Worker::GetCurrentWorker()->OnTaskEnd(); 
    }

    // NOLINTNEXTLINE
    void unhandled_exception() noexcept {
        LOG_TRACE("promise-" << m_id << " unhandled exception");
        m_controller->SaveException(std::current_exception());
        m_controller->OnCoroutineReturn();
		Worker::GetCurrentWorker()->OnTaskEnd(); 
    }

private:
    CorotineControllerSharedPtr m_controller;
    ID<TaskPromise<Empty>> m_id;
};

// It is illegal to have both return_value and return_void in a promise type, even if
// one of them is removed by SFINAE
// https://devblogs.microsoft.com/oldnewthing/20210330-00/?p=105019
template <>
class TaskPromise<void> {
public:
    friend class Task<void>;
    using CorotineControllerSharedPtr = std::shared_ptr<CoroutineController<void>>;

    TaskPromise() noexcept {
        LOG_TRACE("promise-" << m_id << " created");

        m_controller =
            std::make_shared<CoroutineController<void>>(std::coroutine_handle<TaskPromise>::from_promise(*this));
    }
    virtual ~TaskPromise() { LOG_TRACE("promise-" << m_id << " destroyed"); }

    // NOLINTNEXTLINE
    auto initial_suspend() noexcept {
        LOG_TRACE("promise-" << m_id << " inital_suspend");
        return std::suspend_always{};
    }

    // NOLINTNEXTLINE
    auto final_suspend() noexcept {
        LOG_TRACE("promise-" << m_id << " final_suspend");
        m_controller->OnCoroutineDone();
        return std::suspend_never{};
    }

    // NOLINTNEXTLINE
    Task<void> get_return_object() noexcept;

    // NOLINTNEXTLINE
    void return_void() noexcept {
        LOG_TRACE("promise-" << m_id << " return void");
        m_controller->OnCoroutineReturn();
		Worker::GetCurrentWorker()->OnTaskEnd(); 
    }

    // NOLINTNEXTLINE
    void unhandled_exception() noexcept {
        LOG_TRACE("promise-" << m_id << " unhandled exception");
        m_controller->SaveException(std::current_exception());
        m_controller->OnCoroutineReturn();
		Worker::GetCurrentWorker()->OnTaskEnd(); 
    }

private:
    CorotineControllerSharedPtr m_controller;
    ID<TaskPromise<Empty>> m_id;
};

template <typename ReturnType = void>
class Task : public BaseTask<ReturnType>, IWaiter {
public:
    using promise_type = TaskPromise<ReturnType>;  // NOLINT
    using CorotineControllerSharedPtr = std::shared_ptr<CoroutineController<ReturnType>>;
    using ParentTask = BaseTask<ReturnType>;

    Task(CorotineControllerSharedPtr& _controller) 
        : ParentTask(_controller)
    {
        LOG_TRACE("task-" << ParentTask::m_id << " created");
    }
    virtual ~Task() { LOG_TRACE("task-" << ParentTask::m_id << " destroyed"); }

    Task& RunOnProcessor(int _worker_group_id = PreDefWorkerGroup::Current, const SessionId _the_id = 0, const char* _stat_name = nullptr, const std::source_location& _loc = std::source_location::current()) {
        ITask::SetStatInfo(_stat_name, _loc);
        ParentTask::BaseRunOnProcessor(_worker_group_id, _the_id);
        return *this;
    }

    // NOLINTNEXTLINE
    bool await_ready() const noexcept { return ParentTask::IsDone(); }
    // NOLINTNEXTLINE
    void await_suspend(std::coroutine_handle<> _awaiting_coroutine) noexcept {
        m_parent_coroutine_controller = _awaiting_coroutine;
        auto worker = Worker::GetCurrentWorker();
        m_suspended_task = worker->GetCurrentRunningTask();
        ParentTask::WaitReturn(Worker::GetCurrentWorker());
        Worker::GetCurrentWorker()->OnTaskSuspend(this);
    }
    virtual void OnCoroutineReturn() override {
        ParentTask::OnCoroutineReturn();

		Worker::GetCurrentWorker()->OnTaskRun(m_suspended_task); 
        m_suspended_task = nullptr;
        m_parent_coroutine_controller.resume();
    }

    template <typename CheckType = ReturnType>  // NOLINTNEXTLINE
    typename std::enable_if_t<std::is_void_v<CheckType>, void> await_resume() const {
        LOG_TRACE("task-" << ParentTask::m_id << " return void");
        ParentTask::m_controller->CheckException();
    }

    template <typename CheckType = ReturnType>  // NOLINTNEXTLINE
    typename std::enable_if_t<!std::is_void_v<CheckType>, const CheckType>& await_resume() const {
        LOG_TRACE("task-" << ParentTask::m_id << " return value&");
        ParentTask::m_controller->CheckException();
        return ParentTask::m_controller->GetResult();
    }

    // wait for the task to complete in main thread
    void WaitInMain() {
        while (!ParentTask::IsDone()) { Worker::GetCurrentWorker()->Step(); }
    }

private:
    std::coroutine_handle<> m_parent_coroutine_controller;
    ITask* m_suspended_task;
};

template <typename ReturnType>
void CoroutineController<ReturnType>::AddWaitingTask(BaseTask<ReturnType>* _task, Worker* _worker) {
    if (IsDone()) {
        _worker->AddJob(new nd::Job{[_task]() { _task->OnCoroutineReturn(); }});
        return;
    }
    std::lock_guard<std::mutex> lock(m_waiting_tasks_mutex);
    if (m_coroutine == nullptr) {
        _worker->AddJob(new nd::Job{[_task]() { _task->OnCoroutineReturn(); }});
        return;
    }
    m_waiting_tasks.emplace_back(_task, _worker);
}

template <typename ReturnType>
void CoroutineController<ReturnType>::OnCoroutineReturn() {
    // task is waited in other coroutine, so it ought to be exist
    std::lock_guard<std::mutex> lock(m_waiting_tasks_mutex);
    for (auto& waiting_task : m_waiting_tasks) {
        auto* task = std::get<0>(waiting_task);
        auto* worker = std::get<1>(waiting_task);
        worker->AddJob(new nd::Job{[task]() { task->OnCoroutineReturn(); }});
    }
    m_waiting_tasks.clear();
}

template <typename ReturnType>
void CoroutineController<ReturnType>::OnCoroutineDone() {
    std::lock_guard<std::mutex> lock(m_waiting_tasks_mutex);
    if (m_coroutine) {
        // m_coroutine.destroy();
        m_coroutine = nullptr;
    }
}

template <typename ReturnType>
Task<ReturnType> TaskPromise<ReturnType>::get_return_object() noexcept {
    LOG_TRACE("promise-" << m_id << " get_return_object");
    return Task<ReturnType>{m_controller};
}

inline Task<void> TaskPromise<void>::get_return_object() noexcept {
    LOG_TRACE("promise-" << m_id << " get_return_object");
    return Task<void>{m_controller};
}
}  // namespace nd
