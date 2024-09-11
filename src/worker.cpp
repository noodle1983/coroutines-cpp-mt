#include "worker.hpp"
#include "log.hpp"
#include "min_heap.h"

#include <assert.h>

using namespace nd;
using namespace std;

thread_local Worker* Worker::s_current_worker = nullptr; 
thread_local std::thread::id Worker::s_current_thread_id; 
thread_local int Worker::s_current_worker_group_id = PreDefWorkerGroup::Invalid;
thread_local int Worker::s_current_worker_id = 0;
thread_local char Worker::s_worker_name[32] = "";

//-----------------------------------------------------------------------------
Worker::Worker()
    : m_worker_group_id(PreDefWorkerGroup::Invalid)
    , m_worker_id(0)
    , m_worker_num(0)
    , m_is_to_stop(false)
    , m_is_wait_stop(false)
    , m_is_stoped(false)
{
    min_heap_ctor(&m_timer_heap);	
}

//-----------------------------------------------------------------------------

Worker::~Worker()
{
    assert(IsJobQueueEmpty());           // jobs should be empty for a elegant exit!
    assert(min_heap_empty(&m_timer_heap)); // timer heap shoude be empty for a elegant exit!
    min_heap_dtor(&m_timer_heap);	
}

//-----------------------------------------------------------------------------

void Worker::Stop()
{
    m_is_to_stop = true;
    m_queue_cond.notify_one();
}

//-----------------------------------------------------------------------------

void Worker::WaitStop()
{
    m_is_wait_stop = true;
    m_queue_cond.notify_one();
}

//-----------------------------------------------------------------------------

void Worker::WaitUntilEmpty()
{
    while (!IsJobQueueEmpty())
    {
        InternalStep();
    }
}

//-----------------------------------------------------------------------------

void Worker::AddJob(Job* _job) {
  if (m_is_to_stop || m_is_stoped) {
    delete _job;
    return;
  }

  bool job_queue_empty = false;
  {
    lock_guard<mutex> lock(m_queue_mutex);
    job_queue_empty = m_job_queue.empty();
    m_job_queue.push_back(_job);
  }
  if (job_queue_empty) {
    m_queue_cond.notify_one();
  }
}

//-----------------------------------------------------------------------------

TimerHandle Worker::AddLocalTimer(uint64_t _ms_time,
                                  TimerCallback _callback) {
  if (m_is_to_stop || m_is_wait_stop) {
    return NULL;
  }

  bool timer_heap_empty = min_heap_empty(&m_timer_heap) != 0;
  if (128 > min_heap_size(&m_timer_heap)) {
    min_heap_reserve(&m_timer_heap, 128);
  }

  TimerHandle timeout_evt = new min_heap_item_t();
  timeout_evt->callback = _callback;
  timeout_evt->timeout = std::chrono::steady_clock::now() +
                         std::chrono::milliseconds(_ms_time);

  if (-1 == min_heap_push(&m_timer_heap, timeout_evt)) {
    LOG_FATAL("not enough memory!");
    exit(-1);
  }
  if (timer_heap_empty) {
    m_queue_cond.notify_one();
  }
  return timeout_evt;
}

//-----------------------------------------------------------------------------

void Worker::CancelLocalTimer(TimerHandle& _event) {
  if (_event == NULL) {
    return;
  }
  min_heap_erase(&m_timer_heap, _event);
  delete _event;
  _event = NULL;
}

//-----------------------------------------------------------------------------

void Worker::HandleLocalTimer()
{
  if (min_heap_empty(&m_timer_heap) == 0) {
    auto time_now = std::chrono::steady_clock::now();
    while (min_heap_empty(&m_timer_heap) == 0) {
      TimerHandle top_event = min_heap_top(&m_timer_heap);
      if (item_cmp(top_event->timeout, time_now, <=)) {
        min_heap_pop(&m_timer_heap);
        (top_event->callback)();
        delete top_event;
      } else {
        break;
      }
    }
  }
}

//-----------------------------------------------------------------------------

void Worker::ThreadMain()
{
    s_current_worker = this;
    s_current_thread_id = std::this_thread::get_id();
    s_current_worker_group_id = m_worker_group_id;
    s_current_worker_id = m_worker_id;
    if (m_worker_num > 1) {
        snprintf(s_worker_name, sizeof(s_worker_name) - 1, "[%s %d/%d]", m_worker_group_name.c_str(), m_worker_id, m_worker_num);
    }
    else {
        snprintf(s_worker_name, sizeof(s_worker_name) - 1, "[%s]", m_worker_group_name.c_str());
    }
    LOG_TRACE("worker start");

    while (!m_is_to_stop &&!(m_is_wait_stop && IsJobQueueEmpty()))
    {
        InternalStep();
    }
    m_is_stoped = true;
}

//-----------------------------------------------------------------------------

void Worker::InternalStep()
{
    Job* job = NULL;
    {
        lock_guard<mutex> lock(m_queue_mutex);
        if (!m_job_queue.empty())
        {
            job = m_job_queue.front();
            m_job_queue.pop_front();
        }
        else if(m_is_wait_stop)
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
    HandleLocalTimer();

    unique_lock<mutex> queue_lock(m_queue_mutex);
    if (!m_job_queue.empty()) { return; }

    if (!m_is_to_stop && !m_is_wait_stop && m_job_queue.empty() &&
        (min_heap_empty(&m_timer_heap) == 0)) {
      m_queue_cond.wait_for(queue_lock, chrono::microseconds(500));
    } else {
      m_queue_cond.wait_for(queue_lock, chrono::microseconds(10000));
    }
}

//-----------------------------------------------------------------------------

void Worker::Step()
{
    assert(std::this_thread::get_id() == s_current_thread_id);
    InternalStep();
}
