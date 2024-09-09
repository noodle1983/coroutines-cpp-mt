#include "worker.h"
#include "log.h"
#include "min_heap.h"

#include <assert.h>

using namespace nd;
using namespace std;

thread_local Worker* Worker::currentWorkerM = nullptr; 
thread_local std::thread::id Worker::currentThreadId; 
thread_local int Worker::currentWorkerGroupId = PreDefWorkerGroup::Invalid;
thread_local int Worker::currentWorkerId = 0;
thread_local char Worker::workerName[32] = "";

//-----------------------------------------------------------------------------
Worker::Worker()
    : workerGroupIdM(PreDefWorkerGroup::Invalid)
    , workerIdM(0)
    , workerNumM(0)
    , isToStopM(false)
    , isWaitStopM(false)
    , isStopedM(false)
{
    min_heap_ctor(&timerHeapM);	
}

//-----------------------------------------------------------------------------

Worker::~Worker()
{
    min_heap_dtor(&timerHeapM);	
}

//-----------------------------------------------------------------------------

void Worker::stop()
{
    isToStopM = true;
    queueCondM.notify_one();
}

//-----------------------------------------------------------------------------

void Worker::waitStop()
{
    isWaitStopM = true;
    queueCondM.notify_one();
}

//-----------------------------------------------------------------------------

void Worker::waitUntilEmpty()
{
    while (!isJobQueueEmpty())
    {
        internalStep();
    }
}

//-----------------------------------------------------------------------------


void Worker::addJob(Job* theJob)
{
    if (isToStopM || isStopedM){
        delete theJob;
        return;
    }

    bool jobQueueEmpty = false;
    {
        lock_guard<mutex> lock(queueMutexM);
        jobQueueEmpty = jobQueueM.empty();
        jobQueueM.push_back(theJob);
    }
    if (jobQueueEmpty)
    {
        queueCondM.notify_one();
    }
}

//-----------------------------------------------------------------------------

TimerHandle Worker::addLocalTimer(
        uint64_t theMsTime, 
		TimerCallback theCallback)
{
    if (isToStopM || isWaitStopM){
        return NULL;
    }

	bool timerHeapEmpty = min_heap_empty(&timerHeapM);
    if (128 > min_heap_size(&timerHeapM))
    {
        min_heap_reserve(&timerHeapM, 128);
    }

	TimerHandle timeoutEvt = new min_heap_item_t();
    timeoutEvt->callback = theCallback;
    timeoutEvt->timeout = std::chrono::steady_clock::now() + std::chrono::milliseconds(theMsTime);

    if (-1 == min_heap_push(&timerHeapM, timeoutEvt))
    {
        LOG_FATAL("not enough memory!");
        exit(-1);
    }
	if (timerHeapEmpty)
	{
        queueCondM.notify_one();
	}
	return timeoutEvt;
}

//-----------------------------------------------------------------------------

void Worker::cancelLocalTimer(TimerHandle& theEvent)
{
    if (theEvent == NULL) {return;}
    min_heap_erase(&timerHeapM, theEvent);
    delete theEvent;
	theEvent = NULL;
}

//-----------------------------------------------------------------------------

void Worker::handleLocalTimer()
{
    if (!min_heap_empty(&timerHeapM))
    {
        auto timeNow = std::chrono::steady_clock::now();
        while(!min_heap_empty(&timerHeapM)) 
        {
            TimerHandle topEvent = min_heap_top(&timerHeapM);
            if (item_cmp(topEvent->timeout, timeNow, <=))
            {
                min_heap_pop(&timerHeapM);
                (topEvent->callback)();
                delete topEvent;
            }
            else
            { 
                break;
            }
        } 
    }


}

//-----------------------------------------------------------------------------

void Worker::thread_main()
{
    currentWorkerM = this;
    currentThreadId = std::this_thread::get_id();
    currentWorkerGroupId = workerGroupIdM;
    currentWorkerId = workerIdM;
	snprintf(workerName, sizeof(workerName)-1, "[%s %d/%d]", workerGroupNameM.c_str(), workerIdM, workerNumM);
    LOG_TRACE("worker start");

    while (!isToStopM &&!(isWaitStopM && isJobQueueEmpty()))
    {
        internalStep();
    }
    isStopedM = true;
}

//-----------------------------------------------------------------------------

void Worker::internalStep()
{
    Job* job = NULL;
    {
        lock_guard<mutex> lock(queueMutexM);
        if (!jobQueueM.empty())
        {
            job = jobQueueM.front();
            jobQueueM.pop_front();
        }
        else if(isWaitStopM)
        {
            return;
        }
    }

    //handle Job
    if (job != NULL)
    {
        (*job)();
        delete job;
    }

    //handle timer
    handleLocalTimer();

    unique_lock<mutex> queueLock(queueMutexM);
    if (!jobQueueM.empty()) { return; }

    if (!isToStopM && !isWaitStopM && jobQueueM.empty() && !min_heap_empty(&timerHeapM))
    {
        queueCondM.wait_for(queueLock, chrono::microseconds(500));
    }
    else
    {
        queueCondM.wait_for(queueLock, chrono::microseconds(10000));
    }
}

//-----------------------------------------------------------------------------

void Worker::step()
{
    assert(std::this_thread::get_id() == currentThreadId);
    internalStep();
}
