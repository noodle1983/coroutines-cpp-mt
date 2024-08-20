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
        {}
        virtual ~Task(){
        }

        Task& runOnProcessor(int workerGroupId, const SessionId theId = 0)
        {
            if (coroutineM){
				g_worker_mgr->runOnWorkerGroup(workerGroupId, theId, new nd::Job{[this](){
					coroutineM.resume();
                    //notify
				}});
            }
            return *this;
        }


        void wait()
        {
            while (coroutineM && !coroutineM.done()){
                Worker::getCurrentWorker()->step();
            }
        }

    private:
        std::coroutine_handle<TaskPromise> coroutineM;
                
    };
}

