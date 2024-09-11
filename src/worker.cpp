#include "worker.hpp"
#include "log.hpp"
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
    assert(isJobQueueEmpty());           // jobs should be empty for a elegant exit!
    assert(min_heap_empty(&timerHeapM)); // timer heap shoude be empty for a elegant exit!
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

void Worker::addJob(Job* the_job) {
  if (isToStopM || isStopedM) {
    delete the_job;
    return;
  }

  bool job_queue_empty = false;
  {
    lock_guard<mutex> lock(queueMutexM);
    job_queue_empty = jobQueueM.empty();
    jobQueueM.push_back(the_job);
  }
  if (job_queue_empty) {
    queueCondM.notify_one();
  }
}

//-----------------------------------------------------------------------------

TimerHandle Worker::addLocalTimer(uint64_t the_ms_time,
                                  TimerCallback the_callback) {
  if (isToStopM || isWaitStopM) {
    return NULL;
  }

  bool timer_heap_empty = min_heap_empty(&timerHeapM) != 0;
  if (128 > min_heap_size(&timerHeapM)) {
    min_heap_reserve(&timerHeapM, 128);
  }

  TimerHandle timeout_evt = new min_heap_item_t();
  timeout_evt->callback = the_callback;
  timeout_evt->timeout =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(the_ms_time);

  if (-1 == min_heap_push(&timerHeapM, timeout_evt)) {
    LOG_FATAL("not enough memory!");
    exit(-1);
  }
  if (timer_heap_empty) {
    queueCondM.notify_one();
  }
  return timeout_evt;
}

//-----------------------------------------------------------------------------

void Worker::cancelLocalTimer(TimerHandle& the_event) {
  if (the_event == NULL) {
    return;
  }
  min_heap_erase(&timerHeapM, the_event);
  delete the_event;
  the_event = NULL;
}

//-----------------------------------------------------------------------------

void Worker::handleLocalTimer()
{
  if (min_heap_empty(&timerHeapM) == 0) {
    auto time_now = std::chrono::steady_clock::now();
    while (min_heap_empty(&timerHeapM) == 0) {
      TimerHandle top_event = min_heap_top(&timerHeapM);
      if (item_cmp(top_event->timeout, time_now, <=)) {
        min_heap_pop(&timerHeapM);
        (top_event->callback)();
        delete top_event;
      } else {
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
    if (workerNumM > 1) {
        snprintf(workerName, sizeof(workerName) - 1, "[%s %d/%d]", workerGroupNameM.c_str(), workerIdM, workerNumM);
    }
    else {
        snprintf(workerName, sizeof(workerName) - 1, "[%s]", workerGroupNameM.c_str());
    }
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

    unique_lock<mutex> queue_lock(queueMutexM);
    if (!jobQueueM.empty()) { return; }

    if (!isToStopM && !isWaitStopM && jobQueueM.empty() &&
        (min_heap_empty(&timerHeapM) == 0)) {
      queueCondM.wait_for(queue_lock, chrono::microseconds(500));
    } else {
      queueCondM.wait_for(queue_lock, chrono::microseconds(10000));
    }
}

//-----------------------------------------------------------------------------

void Worker::step()
{
    assert(std::this_thread::get_id() == currentThreadId);
    internalStep();
}
