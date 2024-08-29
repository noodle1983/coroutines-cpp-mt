#ifndef WORKER_H
#define WORKER_H

#include <functional>
#include <condition_variable>
#include <list>
#include <thread>
#include <atomic>
#include <cassert>

#include <processor_types.h>
#include <singleton.hpp>
#include <min_heap.h>

typedef void (*TimerCallback)(void *arg);

namespace nd
{
    class Worker
    {
    public:
        Worker();
        ~Worker();

        void init(int theWorkerGroupId, int theWorkerId, int theThreadNum, std::string& theGroupName) {
            // init once only
            assert(workerGroupIdM == PreDefWorkerGroup::Invalid);

            workerGroupIdM = theWorkerGroupId;
            workerIdM = theWorkerId;
            workerNumM = theThreadNum;
            workerGroupNameM = theGroupName;
        }

        inline bool isJobQueueEmpty()
        {
            std::lock_guard<std::mutex> lock(queueMutexM);
            return jobQueueM.empty();
        }
		inline size_t getQueueSize()
		{
            std::lock_guard<std::mutex> lock(queueMutexM);
            return jobQueueM.size();
        }

        static inline void markMainThread(){
            // init once only
            assert(currentWorkerGroupId == PreDefWorkerGroup::Invalid);

            currentThreadId = std::this_thread::get_id();
            currentWorkerGroupId = PreDefWorkerGroup::Main;
            currentWorkerId = 0;
            currentWorkerM = getMainWorker();
            snprintf(workerName, sizeof(workerName)-1, "[Main]");
        }
        static inline Worker* getMainWorker(){
            assert(std::this_thread::get_id() == currentThreadId);
            // a complete worker object but no thread context
            // had to run step() in main to run all the jobs in the queue
            return Singleton<Worker, 0>::instance();
        }
        static inline Worker* getCurrentWorker(){ 
            assert(currentWorkerM != nullptr);
            return currentWorkerM;
        }
        static inline const char* getCurrWorkerName() {
            assert(currentWorkerM != nullptr);
            return workerName;
        }

        void stop();
        void waitStop();
        void waitUntilEmpty();

        void addJob(Job* theJob);
		min_heap_item_t* addLocalTimer(
				uint64_t theMsTime, 
				TimerCallback theCallback,
				void* theArg);
		void cancelLocalTimer(min_heap_item_t*& theEvent);

        //thread runable function
        void thread_main();

        void step();
		void handleLocalTimer();

    private:
        void internalStep();
        thread_local static Worker *currentWorkerM; 
        thread_local static std::thread::id currentThreadId;
        thread_local static int currentWorkerGroupId;
        thread_local static int currentWorkerId;
        thread_local static char workerName[32];

        int workerGroupIdM;
        int workerIdM;
        int workerNumM;
        std::string workerGroupNameM;

        JobQueue jobQueueM;
        std::mutex queueMutexM;
        std::mutex nullMutexM;
        std::condition_variable queueCondM;

		//integrate timer handling
		min_heap_t timerHeapM;

        std::atomic<bool> isToStopM;
        std::atomic<bool> isWaitStopM;
        std::atomic<bool> isStopedM;
    };
}

#endif /* WORKER_H */

