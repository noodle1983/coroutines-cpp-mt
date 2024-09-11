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
        size_t id() const { return m_id; }

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
        CoroutineController(std::coroutine_handle<> coroutine) : m_coroutine(coroutine) {}
        ~CoroutineController() {
            if (m_coroutine) {
                m_coroutine.destroy();
				m_coroutine = nullptr;
            }
        }

        // please make sure the handle is valid before use it
        // it is only used in resume
        std::coroutine_handle<> handle() const { return m_coroutine; }

        void addWaitingTask(BaseTask* task, Worker* worker);
        void onCoroutineReturn();
		void onCoroutineDone();


        bool isDone() {
			std::lock_guard<std::mutex> lock(m_waiting_tasks_mutex);
            return m_coroutine == nullptr;
        }

        virtual void saveResult() {};

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
        BaseTask(CorotineControllerSharedPtr& controller) 
            : m_controller(controller) 
            , m_running_worker(nullptr)
        {
        }
        virtual ~BaseTask() {
        }

        void baseRunOnProcessor(int workerGroupId = PreDefWorkerGroup::Current, const SessionId theId = 0)
        {
            if (!m_controller) { return; }
            if (m_running_worker != nullptr) {
                //LOG_WARN("task can't run twice");
                return;
            }

			m_running_worker = g_worker_mgr->getWorker(workerGroupId, theId);
			baseResume();
            return;
        }

        void baseResume() {
            if (!m_controller) {
                return;
            }
            if (m_running_worker == nullptr) {
                return;
            }

            auto controller = m_controller;
            auto id = m_id.id();
			m_running_worker->addJob(new nd::Job{[controller, id](){
                if (!controller) { return; }
				LOG_TRACE("task-" << id << " run in worker");
				controller->handle().resume();
			}});
            return;
        }

        void waitReturn(Worker* worker) {
            if (!m_controller) {
                return;
            }
            m_controller->addWaitingTask(this, worker);
        }

        virtual void onCoroutineReturn() { }


        bool isDone() const {
            return m_controller == nullptr || m_controller->isDone();
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

        auto initial_suspend() noexcept { 
			LOG_TRACE("promise-" << m_id << " inital_suspend");
            return std::suspend_always{}; 
        }
        auto final_suspend() noexcept { 
			LOG_TRACE("promise-" << m_id << " final_suspend");
            m_controller->onCoroutineDone();
            return std::suspend_never{};
        }

        Task get_return_object() noexcept;

        void return_void() noexcept {
			LOG_TRACE("promise-" << m_id << " return void");
            m_controller->onCoroutineReturn();
        }
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
        using promise_type = TaskPromise;
        Task(CorotineControllerSharedPtr& controller)
            : BaseTask(controller)
        {
			LOG_TRACE("task-" << m_id << " created");
        }
        virtual ~Task(){
			LOG_TRACE("task-" << m_id << " destroyed");
        }

        Task& runOnProcessor(int workerGroupId = PreDefWorkerGroup::Current, const SessionId theId = 0)
        {
            baseRunOnProcessor(workerGroupId, theId);
            return *this;
        }

        bool await_ready() const noexcept { return isDone(); }
        void await_suspend(std::coroutine_handle<> awaitingCoroutineController) noexcept {
            parentCoroutineControllerM = awaitingCoroutineController;
            waitReturn(Worker::getCurrentWorker());
        }
        virtual void onCoroutineReturn () override {
            BaseTask::onCoroutineReturn();

            parentCoroutineControllerM.resume();
        } 
		void await_resume() const noexcept {}


        // wait for the task to complete in main thread
        void waitInMain()
        {
            while (!isDone()) {
                Worker::getCurrentWorker()->step();
            }
        }

    private:
        std::coroutine_handle<> parentCoroutineControllerM;
    };
}

