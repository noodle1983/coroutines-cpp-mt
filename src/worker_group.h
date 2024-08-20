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

		min_heap_item_t* addLocalTimer(
                const SessionId theId, 
				const unsigned long long theMsTime, 
				TimerCallback theCallback,
				void* theArg)
        {
            if (NULL == workersM) {return NULL;}
            unsigned workerId = theId % threadCountM;
            return workersM[workerId].addLocalTimer(theMsTime, theCallback, theArg);
        }
		inline void cancelLocalTimer(
                const SessionId theId, 
                min_heap_item_t*& theEvent)
        {
            if (NULL == workersM) {return;}
            unsigned workerId = theId % threadCountM;
            return workersM[workerId].cancelLocalTimer(theEvent);
        }

        void process(
                const SessionId theId, 
                Job* theJob)
        {
            if (NULL == workersM) {delete theJob; return;}
            unsigned workerId = theId % threadCountM;
            workersM[workerId].process(theJob);
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

