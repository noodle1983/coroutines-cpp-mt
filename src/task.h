#include <atomic>
#include <coroutine>
#include <iostream>

#include <Processor.h>

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
        template<ProcessorGroup theProcessGroup = (ProcessorGroup)PreDefProcessGroup::CurrentWorker>
        void runOnProcessor(const SessionId theId){
            if (processorGroupM != (ProcessorGroup)PreDefProcessGroup::Invalid){
                // already started
                return;
            }
            processorGroupM = theProcessGroup;
            sessionIdM = theId;
            Processor::process<theProcessGroup>(theId, new nd::Job{[this](){
                std::coroutine_handle<TaskPromise>::from_promise(*this).resume();
            }});
        }

        std::atomic<ProcessorGroup> processorGroupM = (ProcessorGroup)PreDefProcessGroup::Invalid;
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
            if (coroutineM){
                coroutineM.destroy();
                coroutineM = nullptr;
            }
        }

        template<ProcessorGroup theProcessGroup = (ProcessorGroup)PreDefProcessGroup::CurrentWorker>
        Task& runOnProcessor(const SessionId theId = 0)
        {
            if (coroutineM){
                coroutineM.promise().runOnProcessor<theProcessGroup>(theId);
            }
            return *this;
        }


        void wait()
        {
            while (coroutineM && !coroutineM.done()){
                CppWorker::getCurrentWorker()->step();
            }
        }

    private:
        std::coroutine_handle<TaskPromise> coroutineM;
                
    };
}

