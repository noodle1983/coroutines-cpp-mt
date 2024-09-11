#ifndef WORKER_GROUP_H
#define WORKER_GROUP_H

#include "worker_types.hpp"
#include "worker.hpp"
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

		WorkerGroup(unsigned _group_id, unsigned _thread_count,
					const std::string& _name = "xxx");
		~WorkerGroup();

		void Start(bool _to_wait_stop = false);
		// must not call stop in its worker
		void WaitStop();
		void Stop();

		Worker* GetWorker(const size_t _session_id) {
		  if (NULL == m_workers) {
			return NULL;
		  }
		  unsigned worker_id = _session_id % m_thread_count;
		  return &m_workers[worker_id];
		}

		TimerHandle AddLocalTimer(const SessionId _id,
								  const unsigned long long _ms_time,
								  TimerCallback _callback) {
		  return GetWorker(_id)->AddLocalTimer(_ms_time,
												   _callback);
		}
		void CancelLocalTimer(const SessionId _id,
							  TimerHandle& _event) {
		  return GetWorker(_id)->CancelLocalTimer(_event);
		}

		void AddJob(const SessionId _id, Job* _job) {
		  GetWorker(_id)->AddJob(_job);
		}

    private:
		unsigned m_group_id;
		unsigned m_thread_count;
		Worker* m_workers;
		std::vector<std::thread> m_threads;
		std::string m_name;
		bool m_wait_stop;
		std::mutex m_stop_mutex;
    };

    //template<WorkerGroup theGroup>
    //void process(SessionId theId, Job* job){
    //    auto processor = Singleton<WorkerGroup, theGroup>::Instance();
    //    processor->process(theId, job);
    //}

    //template<>
    //void process<(WorkerGroup)PreDefWorkerGroup::CurrentWorker>(SessionId theId, Job* job);

    //template<>
    //void process<(WorkerGroup)PreDefWorkerGroup::Main>(SessionId theId, Job* job);
}

#endif /* WORKER_GROUP_H */

