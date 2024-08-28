#ifndef WORKER_GROUP_H
#define WORKER_GROUP_H

#include "processor_types.h"
#include "worker.h"
#include "singleton.hpp"

#include <string>
#include <thread>
#include <vector>

namespace nd
{
	class ProcessorSensor;
    class Worker;

    class WorkerGroup
    {
    public:
		friend class nd::ProcessorSensor;

        //template<WorkerGroup theGroup>
        //static void process(SessionId theId, Job* job);

        WorkerGroup(const unsigned theThreadCount);
        WorkerGroup(const std::string& theName, const unsigned theThreadCount);
        ~WorkerGroup();

        void start(bool toWaitStop = false);
        // must not call stop in its worker
        void waitStop();
        void stop();

        inline Worker* getWorker(const size_t sessionId){
            if (NULL == workersM) {return NULL;}
            unsigned workerId = sessionId % threadCountM;
            return &workersM[workerId];
        }

		min_heap_item_t* addLocalTimer(
                const SessionId theId, 
				const unsigned long long theMsTime, 
				TimerCallback theCallback,
				void* theArg)
        {
            return getWorker(theId)->addLocalTimer(theMsTime, theCallback, theArg);
        }
		inline void cancelLocalTimer(
                const SessionId theId, 
                min_heap_item_t*& theEvent)
        {
            return getWorker(theId)->cancelLocalTimer(theEvent);
        }

        void addJob(
                const SessionId theId, 
                Job* theJob)
        {
            getWorker(theId)->addJob(theJob);
        }

    private:
        unsigned groupIdM;
        unsigned threadCountM;
        Worker* workersM;
        std::vector<std::thread> threadsM;
        std::string nameM;
        bool waitStopM; 
        std::mutex stopMutexM;
    };

    //template<WorkerGroup theGroup>
    //void process(SessionId theId, Job* job){
    //    auto processor = Singleton<WorkerGroup, theGroup>::instance();
    //    processor->process(theId, job);
    //}

    //template<>
    //void process<(WorkerGroup)PreDefProcessGroup::CurrentWorker>(SessionId theId, Job* job);

    //template<>
    //void process<(WorkerGroup)PreDefProcessGroup::Main>(SessionId theId, Job* job);
}

#endif /* WORKER_GROUP_H */

