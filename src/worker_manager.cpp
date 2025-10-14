#include "worker_manager.hpp"

#include <memory>

using namespace nd;

//-----------------------------------------------------------------------------

void WorkerManager::RunOnAllThreads(Job* _job, int _except_group_id) {
	auto worker_manager = g_worker_mgr;
	auto current_worker = Worker::GetCurrentWorker();
	auto main_worker = Worker::GetMainWorker();
	std::unique_ptr<Job> h(_job);

	for (int i = 0; i < worker_manager->m_max_worker_group; ++i) {
		auto group = g_worker_mgr->m_worker_groups[i];
		if (group == nullptr) { continue; }

		for (unsigned j = 0; j < group->m_thread_count; ++j) {
			auto worker = &group->m_workers[j];
			if (worker == nullptr) { continue; }
			if (_except_group_id == PreDefWorkerGroup::CurrentWorker && worker == current_worker) { continue; }

			worker->AddJob(new Job(*_job));
		}
	}
	if (_except_group_id == PreDefWorkerGroup::MainWorker) { return; }

	if (main_worker == nullptr) { return; }
	if (_except_group_id == PreDefWorkerGroup::CurrentWorker && main_worker == current_worker) { return; }

	main_worker->AddJob(new Job(*_job));
}

//-----------------------------------------------------------------------------

void WorkerManager::RunOnGroupWorkers(int _group_id, Job* _job, int _except_group_id)
{
	std::unique_ptr<Job> h(_job);
	auto current_worker = Worker::GetCurrentWorker();
	auto main_worker = Worker::GetMainWorker();
	if (_group_id == PreDefWorkerGroup::MainWorker) { 
		if (_except_group_id == PreDefWorkerGroup::MainWorker) { return; }

		if (main_worker == nullptr) { return; }
		if (_except_group_id == PreDefWorkerGroup::CurrentWorker && main_worker == current_worker) { return; }

		main_worker->AddJob(new Job(*_job));
		return;
	}

	if (_group_id == PreDefWorkerGroup::CurrentWorker) {
		if (_except_group_id == PreDefWorkerGroup::CurrentWorker) { return; }
		if (_except_group_id == PreDefWorkerGroup::MainWorker && main_worker == current_worker) { return; }

		current_worker->AddJob(new Job(*_job));
		return;
	}
	if (_group_id == _except_group_id) { return; }

	auto worker_manager = g_worker_mgr;
	if (_group_id < 0 || _group_id > worker_manager->m_max_worker_group) { return; }

	auto group = g_worker_mgr->m_worker_groups[_group_id];
	if (group == nullptr) { return; }

	for (unsigned j = 0; j < group->m_thread_count; ++j) {
		auto worker = &group->m_workers[j];
		if (worker == nullptr) { continue; }
		if (_except_group_id == PreDefWorkerGroup::CurrentWorker && worker == current_worker) { continue; }

		worker->AddJob(new Job(*_job));
	}

}

//-----------------------------------------------------------------------------
