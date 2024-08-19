/**
 * Licensing Information
 *    This is a release of rzsz-nd, brought to you by Dong Lu(noodle1983@126
 *    .com). Except for extra permissions from Dong Lu(noodle1983@126.com),
 *    this software is released under version 3 of the GNU General
 *    Public License (GPLv3).
 **/
#ifndef CPPWORKER_H
#define CPPWORKER_H

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
    class CppWorker
    {
    public:
        CppWorker();
        ~CppWorker();

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

        static inline void markMainThread(){currentThreadId = std::this_thread::get_id();}
        static inline CppWorker* getMainWorker(){
            assert(currentWorkerM == nullptr);
            assert(std::this_thread::get_id() == currentThreadId);
            return Singleton<CppWorker, 0>::instance();
        }
        static inline CppWorker* getCurrentWorker(){ return currentWorkerM ? currentWorkerM : getMainWorker(); }

        void stop();
        void waitStop();

        void process(Job* theJob);
		min_heap_item_t* addLocalTimer(
				uint64_t theMsTime, 
				TimerCallback theCallback,
				void* theArg);
		void cancelLocalTimer(min_heap_item_t*& theEvent);

        //thread runable function
        void run();

        void step();
		void handleLocalTimer();

    private:
        void internalStep();
        thread_local static CppWorker *currentWorkerM; 
        thread_local static std::thread::id currentThreadId;

        JobQueue jobQueueM;
        std::mutex queueMutexM;
        std::mutex nullMutexM;
        std::condition_variable queueCondM;

		//integrate timer handling
		min_heap_t timerHeapM;

        std::atomic<size_t> isToStopM;
        std::atomic<size_t> isWaitStopM;
    };
}

#endif /* CPPWORKER_H */

