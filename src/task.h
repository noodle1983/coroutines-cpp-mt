#include <atomic>
#include <coroutine>
#include <iostream>

#include "processor_types.h"
#include "worker_manager.h"

namespace nd
{
    //-----------------------------------------
    // no result, no except
    //-----------------------------------------
    class Task;
    class TaskPromise
    {
    public:
        friend class Task;

        TaskPromise() noexcept {}
        virtual ~TaskPromise() {}

        auto initial_suspend() noexcept { return std::suspend_always{}; }
        auto final_suspend() noexcept { return std::suspend_always(); }

        Task get_return_object() noexcept;

        void return_void() noexcept {}
        void unhandled_exception() noexcept { }

    private:
        void runOnProcessor(int workerGroupId, const SessionId theId){
        }

        SessionId sessionIdM = 0;
    };

    class Task
    {
    public:
        using promise_type = TaskPromise;
        Task(std::coroutine_handle<TaskPromise> theCoroutine)
            : coroutineM(theCoroutine)
            , startWorkerM(Worker::getCurrentWorker())
        {}
        virtual ~Task(){
        }

        Task& runOnProcessor(int workerGroupId = PreDefProcessGroup::Current, const SessionId theId = 0)
        {
            if (coroutineM){
                runningWorkerM = g_worker_mgr->getWorker(workerGroupId, theId);
                resume();
            }
            return *this;
        }

        Task& resume() {
            if (coroutineM == nullptr || runningWorkerM == nullptr) {
                return *this;
            }

			runningWorkerM->addJob(new nd::Job{[this](){
				coroutineM.resume();

				//notify
				startWorkerM->addJob(new nd::Job{ [this]() {
					if (parentCoroutineM) {
						parentCoroutineM.resume();
						return;
					}

					if (!coroutineM.done()) {
                        resume();
					}
				}});
			}});
            return *this;
        }

        // awaitable functions, for the task in a coroutine
        bool await_ready() const noexcept { return coroutineM.done(); }
        void await_suspend(std::coroutine_handle<> awaitingCoroutine) noexcept {
            parentCoroutineM = awaitingCoroutine;
        }
		void await_resume() const noexcept {}


        // wait for the task to complete in main thread
        void waitInMain()
        {
            while (coroutineM && !coroutineM.done()){
                Worker::getCurrentWorker()->step();
            }
        }

    private:
        // the coroutine that represents this task
        std::coroutine_handle<TaskPromise> coroutineM;

        // the worker that started this task, it must lives longer than the task
        nd::Worker *startWorkerM = nullptr;

        // the worker that is running/to run this task
        nd::Worker *runningWorkerM = nullptr;

        std::coroutine_handle<> parentCoroutineM;


                
    };
}

