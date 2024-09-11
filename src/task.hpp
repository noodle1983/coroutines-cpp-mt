#include "worker_types.hpp"
#include "worker_manager.hpp"
#include "log.hpp"

#include <atomic>
#include <coroutine>
#include <iostream>
#include <list>
#include <tuple>


namespace nd
{
    class CoroutineController;
    using CorotineControllerSharedPtr = std::shared_ptr<CoroutineController>;
    using CorotineControllerWeakPtr = std::weak_ptr<CoroutineController>;
    class BaseTask;

    template<typename T>
    class ID
    {
    public:
        ID() : m_id(++s_id) {}
        operator size_t() const { return m_id; }
        size_t Id() const { return m_id; }

       private:
        static std::atomic<size_t> s_id;
        size_t m_id;
    };
    template<typename T>
    std::atomic<size_t> ID<T>::s_id = 0;

    //-----------------------------------------
    // Control the lifetime between Task and coroutine_handle
    //-----------------------------------------
    class CoroutineController {
    public:
     CoroutineController(std::coroutine_handle<> _coroutine)
         : m_coroutine(_coroutine) {}
     ~CoroutineController() {
       if (m_coroutine) {
         m_coroutine.destroy();
         m_coroutine = nullptr;
       }
        }

        // please make sure the handle is valid before use it
        // it is only used in resume
        std::coroutine_handle<> Handle() const { return m_coroutine; }

        void AddWaitingTask(BaseTask* _task, Worker* _worker);
        void OnCoroutineReturn();
        void OnCoroutineDone();

        bool IsDone() {
          std::lock_guard<std::mutex> lock(m_waiting_tasks_mutex);
          return m_coroutine == nullptr;
        }

        virtual void SaveResult() {};

       private:
        ID<CoroutineController> m_id;

        using WaitingTask = std::tuple<BaseTask*, Worker*>;
        std::list<WaitingTask> m_waiting_tasks;
        std::mutex m_waiting_tasks_mutex;

        std::coroutine_handle<> m_coroutine;
    };

    class BaseTask {
    public:
        friend class CoroutineController;
        BaseTask(CorotineControllerSharedPtr& _controller)
            : m_controller(_controller), m_running_worker(nullptr) {}
        virtual ~BaseTask() {
        }

        void BaseRunOnProcessor(
            int _worker_group_id = PreDefWorkerGroup::Current,
            const SessionId _the_id = 0) {
          if (!m_controller) {
            return;
          }
          if (m_running_worker != nullptr) {
            // LOG_WARN("task can't run twice");
            return;
          }

          m_running_worker = g_worker_mgr->GetWorker(_worker_group_id, _the_id);
          BaseResume();
        }

        void BaseResume() {
          if (!m_controller) {
            return;
          }
          if (m_running_worker == nullptr) {
            return;
          }

          auto controller = m_controller;
          auto id = m_id.Id();
          m_running_worker->AddJob(new nd::Job{[controller, id]() {
            if (!controller) {
              return;
            }
            LOG_TRACE("task-" << id << " run in worker");
            controller->Handle().resume();
          }});
        }

        void WaitReturn(Worker* _worker) {
          if (!m_controller) {
            return;
          }
          m_controller->AddWaitingTask(this, _worker);
        }

        virtual void OnCoroutineReturn() {}

        bool IsDone() const {
          return m_controller == nullptr || m_controller->IsDone();
        }

    protected:
        ID<BaseTask> m_id;

        CorotineControllerSharedPtr m_controller;
        nd::Worker* m_running_worker;
    };

    //-----------------------------------------
    // no result, no except
    //-----------------------------------------
    class Task;
    class TaskPromise
    {
    public:
        friend class Task;

        TaskPromise() noexcept {
      LOG_TRACE("promise-" << m_id << " created");

            m_controller = std::make_shared<CoroutineController>(std::coroutine_handle<TaskPromise>::from_promise(*this));
        }
        virtual ~TaskPromise() {
      LOG_TRACE("promise-" << m_id << " destroyed");
        }

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
        Task get_return_object() noexcept;

    // NOLINTNEXTLINE
        void return_void() noexcept {
      LOG_TRACE("promise-" << m_id << " return void");
                        m_controller->OnCoroutineReturn();
        }

    // NOLINTNEXTLINE
        void unhandled_exception() noexcept { 
      LOG_TRACE("promise-" << m_id << " unhandled exception");
        }

    private:
        CorotineControllerSharedPtr m_controller;
        ID<TaskPromise> m_id;
    };

    class Task : public BaseTask
    {
    public:
     using promise_type = TaskPromise; // NOLINT
     Task(CorotineControllerSharedPtr& _controller) : BaseTask(_controller) {
       LOG_TRACE("task-" << m_id << " created");
     }
        virtual ~Task(){
      LOG_TRACE("task-" << m_id << " destroyed");
        }

        Task& RunOnProcessor(int _worker_group_id = PreDefWorkerGroup::Current,
                             const SessionId _the_id = 0) {
          BaseRunOnProcessor(_worker_group_id, _the_id);
          return *this;
        }

                // NOLINTNEXTLINE
        bool await_ready() const noexcept { return IsDone(); }
        // NOLINTNEXTLINE
        void await_suspend(std::coroutine_handle<> _awaiting_coroutine) noexcept {
          m_parent_coroutine_controller = _awaiting_coroutine;
          WaitReturn(Worker::GetCurrentWorker());
        }
        virtual void OnCoroutineReturn() override {
          BaseTask::OnCoroutineReturn();

          m_parent_coroutine_controller.resume();
        }
                // NOLINTNEXTLINE
    void await_resume() const noexcept {}


        // wait for the task to complete in main thread
    void WaitInMain() {
      while (!IsDone()) {
      Worker::GetCurrentWorker()->Step();
      }
    }

    private:
    std::coroutine_handle<> m_parent_coroutine_controller;
    };
}

