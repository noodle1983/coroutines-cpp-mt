/**
 * Licensing Information
 *    This is a release of rzsz-nd, brought to you by Dong Lu(noodle1983@126
 *    .com). Except for extra permissions from Dong Lu(noodle1983@126.com),
 *    this software is released under version 3 of the GNU General
 *    Public License (GPLv3).
 **/
#ifndef CPPPROCESSOR_H
#define CPPPROCESSOR_H

#include "processor_types.h"
#include "cpp_worker.h"
#include "singleton.hpp"

#include <string>
#include <thread>
#include <vector>

namespace nd
{
	class ProcessorSensor;
    class CppWorker;

    class CppProcessor
    {
    public:
		friend class nd::ProcessorSensor;

        template<ProcessorGroup theGroup>
        static void process(SessionId theId, Job* job);

        CppProcessor(const unsigned theThreadCount);
        CppProcessor(const std::string& theName, const unsigned theThreadCount);
        ~CppProcessor();

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
        unsigned threadCountM;
        CppWorker* workersM;
        std::vector<std::thread> threadsM;
        std::string nameM;
        bool waitStopM; 
        std::mutex stopMutexM;
    };

    template<ProcessorGroup theGroup>
    void process(SessionId theId, Job* job){
        auto processor = Singleton<CppProcessor, theGroup>::instance();
        processor->process(theId, job);
    }

    template<>
    void process<(ProcessorGroup)PreDefProcessGroup::CurrentWorker>(SessionId theId, Job* job);

    template<>
    void process<(ProcessorGroup)PreDefProcessGroup::Main>(SessionId theId, Job* job);
}

#endif /* CPPPROCESSOR_H */

