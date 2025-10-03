#include "worker.hpp"

#include "log.hpp"
#include "min_heap.h"
#include "task.hpp"
#include "waiter.hpp"

using namespace nd;
using namespace std;

thread_local Worker* Worker::s_current_worker = nullptr;
thread_local std::thread::id Worker::s_current_thread_id;
thread_local int Worker::s_current_worker_group_id = PreDefWorkerGroup::Invalid;
thread_local int Worker::s_current_worker_id = 0;
thread_local char Worker::s_worker_name[MAX_WORKER_NAME_LEN] = "";


//-----------------------------------------------------------------------------

const char* get_current_worker_name() { return nd::Worker::GetCurrWorkerName(); }

//-----------------------------------------------------------------------------

Worker::Worker()
    : m_worker_group_id(PreDefWorkerGroup::Invalid),
      m_worker_id(0),
      m_worker_num(0),
      m_is_to_stop(false),
      m_is_wait_stop(false),
      m_is_stoped(false),
      m_current_running_task(nullptr){
    min_heap_ctor(&m_timer_heap);
}

//-----------------------------------------------------------------------------

Worker::~Worker() {
    MY_ASSERT(IsJobQueueEmpty(), "jobs should be empty for an elegant exit!");
    MY_ASSERT(min_heap_empty(&m_timer_heap), "timer heap should be empty for an elegant exit!");
    min_heap_dtor(&m_timer_heap);
}

//-----------------------------------------------------------------------------

void Worker::Stop() {
    m_is_to_stop = true;
    m_queue_cond.notify_one();
}

//-----------------------------------------------------------------------------

void Worker::WaitStop() {
    m_is_wait_stop = true;
    m_queue_cond.notify_one();
}

//-----------------------------------------------------------------------------

void Worker::WaitUntilEmpty() {
    while (!IsJobQueueEmpty()) { InternalStep(); }
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
    if (job_queue_empty) { m_queue_cond.notify_one(); }
}

//-----------------------------------------------------------------------------

TimerHandle Worker::AddLocalTimer(uint64_t _ms_time, TimerCallback _callback) {
    if (m_is_to_stop || m_is_wait_stop) { return NULL; }

    bool timer_heap_empty = min_heap_empty(&m_timer_heap) != 0;
    constexpr size_t MIN_HEAP_RESERVE_SIZE = 128;
    if (MIN_HEAP_RESERVE_SIZE > min_heap_size(&m_timer_heap)) {
        min_heap_reserve(&m_timer_heap, MIN_HEAP_RESERVE_SIZE);
    }

    TimerHandle timeout_evt = new min_heap_item_t();
    timeout_evt->callback = _callback;
    timeout_evt->timeout = CppClock::now() + std::chrono::milliseconds(_ms_time);

    if (-1 == min_heap_push(&m_timer_heap, timeout_evt)) {
        LOG_FATAL("not enough memory!");
        exit(-1);
    }
    if (timer_heap_empty ||(min_heap_top(&m_timer_heap) == timeout_evt)) { m_queue_cond.notify_one(); }
    return timeout_evt;
}

//-----------------------------------------------------------------------------

void Worker::CancelLocalTimer(TimerHandle& _event) {
    if (_event == NULL) { return; }
    min_heap_erase(&m_timer_heap, _event);
    delete _event;
    _event = NULL;
}

//-----------------------------------------------------------------------------

CppDuration Worker::HandleLocalTimer() {
    if (min_heap_empty(&m_timer_heap) == 0) {
        auto time_now = CppClock::now();
        while (min_heap_empty(&m_timer_heap) == 0) {
            TimerHandle top_event = min_heap_top(&m_timer_heap);
            if (item_cmp(top_event->timeout, time_now, <=)) {
                //LOG_TRACE("diff time:" << top_event->timeout - time_now);
                min_heap_pop(&m_timer_heap);
                (top_event->callback)();
                delete top_event;
            } else {
                return top_event->timeout - time_now;
            }
        }
    }
    return CppDuration::max();
}

//-----------------------------------------------------------------------------

void Worker::ThreadMain() {
    s_current_worker = this;
    s_current_thread_id = std::this_thread::get_id();
    s_current_worker_group_id = m_worker_group_id;
    s_current_worker_id = m_worker_id;
    if (m_worker_num > 1) {
        snprintf(s_worker_name,
                 MAX_WORKER_NAME_LEN - 1,
                 "[%s %d/%d]",
                 m_worker_group_name.c_str(),
                 m_worker_id,
                 m_worker_num);
    } else {
        snprintf(s_worker_name, MAX_WORKER_NAME_LEN - 1, "[%s]", m_worker_group_name.c_str());
    }
    LOG_TRACE("worker start");

    while (!m_is_to_stop && !(m_is_wait_stop && IsJobQueueEmpty())) { InternalStep(); }
    m_is_stoped = true;
}

//-----------------------------------------------------------------------------

void Worker::InternalStep() {
    Job* job = NULL;
    {
        lock_guard<mutex> lock(m_queue_mutex);
        if (!m_job_queue.empty()) {
            job = m_job_queue.front();
            m_job_queue.pop_front();
        } else if (m_is_wait_stop) {
            return;
        }
    }

    // handle Job
    if (job != NULL) {
        (*job)();
        delete job;
    }

    // handle timer
    auto next_duration = HandleLocalTimer();

    // end of handling
    MY_ASSERT(m_current_running_task == nullptr, "The running task must either in exit or await, thus must be null here.");

    unique_lock<mutex> queue_lock(m_queue_mutex);
    if (!m_job_queue.empty()) { return; }

    const auto MAX_WAIT_TIME_MICROSECONDS = chrono::microseconds(10000);
    const auto& wait_duration = (next_duration < MAX_WAIT_TIME_MICROSECONDS) ? next_duration : MAX_WAIT_TIME_MICROSECONDS;
	m_queue_cond.wait_for(queue_lock, wait_duration);
}

//-----------------------------------------------------------------------------

void Worker::Step() {
    MY_ASSERT(std::this_thread::get_id() == s_current_thread_id, "step can only be run in the worker thread!");
    InternalStep();
}

//-----------------------------------------------------------------------------

void Worker::OnTaskStart(ITask* _task) { 
	MY_ASSERT(_task, "input task can't be null!");
    MY_ASSERT(m_current_running_task == nullptr, "There is another task running, new task must be run in a new job.");

    auto task_id = _task->Id();
	MY_ASSERT(m_tasks.find(task_id) == m_tasks.end(), "task can't be start twice in the same time");
    m_tasks[task_id] = _task;

	OnTaskRun(_task);
}

//-----------------------------------------------------------------------------

void Worker::OnTaskRun(ITask* _task) { 
    MY_ASSERT(m_current_running_task == nullptr, "There is another task running, new task must be run in a new job.");
    m_current_running_task = _task; 
}

//-----------------------------------------------------------------------------

void Worker::OnTaskSuspend(IWaiter* _waiter) { 
    MY_ASSERT(m_current_running_task != nullptr, "There is no task running?!");
    m_current_running_task = nullptr; 
}

//-----------------------------------------------------------------------------

void Worker::OnTaskEnd() { 
    MY_ASSERT(m_current_running_task != nullptr, "no running task");

    auto it = m_tasks.find(m_current_running_task->Id());
    MY_ASSERT(it != m_tasks.end(), "task-%lld is not running?!", m_current_running_task->Id());

    m_tasks.erase(it);
    m_current_running_task = nullptr; 
}

//-----------------------------------------------------------------------------

