#ifndef WORKER_H
#define WORKER_H

#include <functional>
#include <condition_variable>
#include <list>
#include <thread>
#include <atomic>
#include <cassert>

#include <worker_types.hpp>
#include <singleton.hpp>
#include <min_heap.h>

namespace nd
{
    class Worker
    {
    public:
        Worker();
        ~Worker();

        void Init(int _worker_group_id, int _worker_id,
                  int _thread_num, std::string& _group_name) {
          // init once only
          assert(m_worker_group_id == PreDefWorkerGroup::Invalid);

          m_worker_group_id = _worker_group_id;
          m_worker_id = _worker_id;
          m_worker_num = _thread_num;
          m_worker_group_name = _group_name;
        }

        bool IsJobQueueEmpty() {
          std::lock_guard<std::mutex> lock(m_queue_mutex);
          return m_job_queue.empty();
        }
        size_t GetQueueSize() {
          std::lock_guard<std::mutex> lock(m_queue_mutex);
          return m_job_queue.size();
        }

        static void MarkMainThread() {
          // init once only
          assert(s_current_worker_group_id == PreDefWorkerGroup::Invalid);

          s_current_thread_id = std::this_thread::get_id();
          s_current_worker_group_id = PreDefWorkerGroup::Main;
          s_current_worker_id = 0;
          s_current_worker = GetMainWorker();
          snprintf(s_worker_name, sizeof(s_worker_name) - 1, "[main]");
        }
        static Worker* GetMainWorker() {
          assert(std::this_thread::get_id() == s_current_thread_id);
          // a complete worker object but no thread context
          // had to run step() in main to run all the jobs in the queue
          return Singleton<Worker, 0>::Instance();
        }
        static Worker* GetCurrentWorker() {
          assert(s_current_worker != nullptr);
          return s_current_worker;
        }
        static const char* GetCurrWorkerName() {
          assert(s_current_worker != nullptr);
          return s_worker_name;
        }

        void Stop();
        void WaitStop();
        void WaitUntilEmpty();

        void AddJob(Job* _job);
        TimerHandle AddLocalTimer(uint64_t _ms_time,
                                  TimerCallback _callback);
        void CancelLocalTimer(TimerHandle& _event);

        //thread runable function
        void ThreadMain();

        void Step();
        void HandleLocalTimer();

       private:
        void InternalStep();
        thread_local static Worker* s_current_worker;
        thread_local static std::thread::id s_current_thread_id;
        thread_local static int s_current_worker_group_id;
        thread_local static int s_current_worker_id;
        thread_local static char s_worker_name[32];

        int m_worker_group_id;
        int m_worker_id;
        int m_worker_num;
        std::string m_worker_group_name;

        JobQueue m_job_queue;
        std::mutex m_queue_mutex;
        std::mutex m_null_mutex;
        std::condition_variable m_queue_cond;

        // integrate timer handling
        min_heap_t m_timer_heap;

        std::atomic<bool> m_is_to_stop;
        std::atomic<bool> m_is_wait_stop;
        std::atomic<bool> m_is_stoped;
    };
}

#endif /* WORKER_H */

