#include "worker_group.h"
#include "worker.h"

#include <functional>
#include <chrono>

using namespace std;
using namespace nd;

//-----------------------------------------------------------------------------

WorkerGroup::WorkerGroup(const unsigned theThreadCount)
    : threadCountM(theThreadCount)
    , workersM(NULL)
    , nameM("group")
    , waitStopM(false)
{
}

//-----------------------------------------------------------------------------

WorkerGroup::WorkerGroup(const std::string& theName, const unsigned theThreadCount)
    : threadCountM(theThreadCount)
    , workersM(NULL)
    , nameM(theName)
    , waitStopM(false)
{
}

//-----------------------------------------------------------------------------

WorkerGroup::~WorkerGroup()
{
    if (workersM) {
        if (waitStopM){
            waitStop();
        }
        else {
            stop();
        }
    }
}

//-----------------------------------------------------------------------------

void WorkerGroup::start(bool toWaitStop)
{
    waitStopM = toWaitStop;
    if (0 == threadCountM)
        return;

    if (NULL != workersM)
        return;

    workersM = new Worker[threadCountM];
    threadsM.reserve(threadCountM);
    for (unsigned i = 0; i < threadCountM; i++)
    {
        workersM[i].init(groupIdM, threadCountM, i);
        threadsM.push_back(thread(&Worker::thread_main, &workersM[i]));
    }
}

//-----------------------------------------------------------------------------

void WorkerGroup::waitStop()
{
    lock_guard<mutex> lock(stopMutexM);
    if (NULL == workersM)
        return;

    unsigned int i = 0;
    while(true)
    {
        /* check the worker once only */
        if(i < threadCountM && workersM[i].isJobQueueEmpty())
        {
            workersM[i].waitStop();
            i++;
        }
        if (i == threadCountM)
        {
            break;
        }
        else
        {
            this_thread::sleep_for(chrono::milliseconds(1));
        }
    }
    for (unsigned i = 0; i < threadCountM; i++)
    {
        threadsM[i].join();
    }
    delete []workersM;
    workersM = NULL;
}

//-----------------------------------------------------------------------------

void WorkerGroup::stop()
{
    lock_guard<mutex> lock(stopMutexM);
    if (NULL == workersM)
        return;

    for (unsigned i = 0; i < threadCountM; i++)
    {
        workersM[i].stop();
    }
    for (unsigned i = 0; i < threadCountM; i++)
    {
        threadsM[i].join();
    }
    delete []workersM;
    workersM = NULL;
}

//-----------------------------------------------------------------------------

//template<>
//void WorkerGroup::process<(WorkerGroup)PreDefProcessGroup::CurrentWorker>(SessionId theId, Job* job){
//    Worker::getCurrentWorker()->process(job);
//}
//
////-----------------------------------------------------------------------------
//
//template<>
//void WorkerGroup::process<(WorkerGroup)PreDefProcessGroup::Main>(SessionId theId, Job* job){
//    Worker::getMainWorker()->process(job);
//}

//-----------------------------------------------------------------------------
