#include "worker_group.hpp"
#include "worker.hpp"

#include <functional>
#include <chrono>

using namespace std;
using namespace nd;

//-----------------------------------------------------------------------------

WorkerGroup::WorkerGroup(unsigned _group_id,
                         const unsigned _thread_count,
                         const std::string& _name)
    : m_group_id(_group_id),
      m_thread_count(_thread_count),
      m_workers(NULL),
      m_name(_name),
      m_wait_stop(false) {}

//-----------------------------------------------------------------------------

WorkerGroup::~WorkerGroup()
{
  if (m_workers != nullptr) {
    if (m_wait_stop) {
      WaitStop();
    } else {
      Stop();
    }
  }
}

//-----------------------------------------------------------------------------

void WorkerGroup::Start(bool _to_wait_stop) {
  m_wait_stop = _to_wait_stop;
  if (0 == m_thread_count) {
    return;
  }

  if (NULL != m_workers) {
    return;
  }

  m_workers = new Worker[m_thread_count];
  m_threads.reserve(m_thread_count);
  for (unsigned i = 0; i < m_thread_count; i++) {
    m_workers[i].Init(m_group_id, i, m_thread_count, m_name);
    m_threads.push_back(thread(&Worker::ThreadMain, &m_workers[i]));
  }
}

//-----------------------------------------------------------------------------

void WorkerGroup::WaitStop()
{
    lock_guard<mutex> lock(m_stop_mutex);
    if (NULL == m_workers) {
      return;
    }

    unsigned int i = 0;
    while(true)
    {
        /* check the worker once only */
        if(i < m_thread_count && m_workers[i].IsJobQueueEmpty())
        {
            m_workers[i].WaitStop();
            i++;
        }
        if (i == m_thread_count)
        {
            break;
        }

        this_thread::sleep_for(chrono::milliseconds(1));
    }
    for (unsigned i = 0; i < m_thread_count; i++)
    {
        m_threads[i].join();
    }
    delete []m_workers;
    m_workers = NULL;
}

//-----------------------------------------------------------------------------

void WorkerGroup::Stop()
{
    lock_guard<mutex> lock(m_stop_mutex);
    if (NULL == m_workers) {
      return;
    }

    for (unsigned i = 0; i < m_thread_count; i++)
    {
        m_workers[i].Stop();
    }
    for (unsigned i = 0; i < m_thread_count; i++)
    {
        m_threads[i].join();
    }
    delete []m_workers;
    m_workers = NULL;
}

//-----------------------------------------------------------------------------

//template<>
//void WorkerGroup::process<(WorkerGroup)PreDefWorkerGroup::CurrentWorker>(SessionId theId, Job* job){
//    Worker::GetCurrentWorker()->process(job);
//}
//
////-----------------------------------------------------------------------------
//
//template<>
//void WorkerGroup::process<(WorkerGroup)PreDefWorkerGroup::Main>(SessionId theId, Job* job){
//    Worker::GetMainWorker()->process(job);
//}

//-----------------------------------------------------------------------------
