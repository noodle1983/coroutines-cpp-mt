#ifndef WORKER_MANAGER_H
#define WORKER_MANAGER_H

#include "singleton.hpp"
#include "worker_group.hpp"
#include "worker.hpp"

#include <assert.h>
#include <string.h>

namespace nd{
	class WorkerManager{
		enum {MAX_WORKER_GROUP = 10};

	public:
		WorkerManager() 
			: m_max_worker_group(0)
			, m_worker_groups(nullptr) {
		}

		~WorkerManager() {
			stopAll();
		}

		void init(unsigned max_worker_group) {
			assert(max_worker_group <= MAX_WORKER_GROUP);
			m_max_worker_group = max_worker_group;

			m_worker_groups = new WorkerGroup*[m_max_worker_group];
			memset(m_worker_groups, 0, sizeof(WorkerGroup*) * m_max_worker_group);
		}

		void start(unsigned worker_group_id, signed processor_num, const std::string& theName = "xxx") {
			assert(worker_group_id < m_max_worker_group);
			assert(m_worker_groups[worker_group_id] == nullptr);

			m_worker_groups[worker_group_id] = new WorkerGroup(worker_group_id, processor_num, theName);
			m_worker_groups[worker_group_id]->Start();
		}

		void stop(unsigned worker_group_id) {
			assert(worker_group_id < m_max_worker_group);
			if (m_worker_groups[worker_group_id] == nullptr) {
				return;
			}

			m_worker_groups[worker_group_id]->WaitStop();
			delete m_worker_groups[worker_group_id];
			m_worker_groups[worker_group_id] = nullptr;
		}

		void stopAll() {
			if (m_max_worker_group == 0) { return; }
			if (m_worker_groups == nullptr) { return; }

			for (unsigned i = 0; i < m_max_worker_group; ++i) {
				if (m_worker_groups[i] == nullptr) {
					continue;
				}

				stop(i);
			}
			delete[] m_worker_groups;
			m_worker_groups = nullptr;
		}

		Worker* getWorker(unsigned worker_group_id, size_t session_id) {
			if (worker_group_id == PreDefWorkerGroup::Main) {
				return Worker::GetMainWorker();
			}
			if (worker_group_id == PreDefWorkerGroup::Current) {
				return Worker::GetCurrentWorker();
			}

			assert(0 <= worker_group_id && worker_group_id < m_max_worker_group);
			assert(m_worker_groups[worker_group_id] != nullptr);

			return m_worker_groups[worker_group_id]->GetWorker(session_id);
		}

		void runOnWorkerGroup(int worker_group_id, size_t session_id, Job* job) {
			if (worker_group_id == PreDefWorkerGroup::Main) {
				return runOnMainThread(job);
			}
			if (worker_group_id == PreDefWorkerGroup::Current) {
				return runOnCurrentThread(job);
			}

			assert(0 <= worker_group_id && worker_group_id < m_max_worker_group);
			assert(m_worker_groups[worker_group_id] != nullptr);
			m_worker_groups[worker_group_id]->AddJob(session_id, job);
		}
		
		void runOnMainThread(Job* job) {
			Worker::GetMainWorker()->AddJob(job);
		}

		void runOnCurrentThread(Job* job) {
			Worker::GetCurrentWorker()->AddJob(job);
		}


	protected:
		size_t m_max_worker_group;
		WorkerGroup** m_worker_groups;

		
	};
};

#define g_worker_mgr (nd::Singleton<nd::WorkerManager, 0>::Instance())


#endif /* WORKER_MANAGER_H */

