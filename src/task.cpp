#include "Task.h"

using namespace nd;

void CoroutineController::addWaitingTask(BaseTask* task, Worker* worker) {
	if (isDone()) {
		worker->addJob(new nd::Job{ [task]() {
			task->onCoroutineReturn();
		} });
		return;
	}
	std::lock_guard<std::mutex> lock(m_waiting_tasks_mutex);
	if (isDone()) {
		worker->addJob(new nd::Job{ [task]() {
			task->onCoroutineReturn();
		} });
		return;
	}
	m_waiting_tasks.emplace_back(task, worker);
}

void CoroutineController::onCoroutineReturn() {
	saveResult();

	{
		// task is waited in other coroutine, so it ought to be exist
		std::lock_guard<std::mutex> lock(m_waiting_tasks_mutex);
		for (auto& waiting_task : m_waiting_tasks) {
			auto task = std::get<0>(waiting_task);
			auto worker = std::get<1>(waiting_task);
			worker->addJob(new nd::Job{ [task]() {
				task->onCoroutineReturn();
			} });
		}
		m_waiting_tasks.clear();
	}

}

void CoroutineController::onCoroutineDone() {
	std::lock_guard<std::mutex> lock(m_waiting_tasks_mutex);
	m_coroutine = nullptr;
}



Task TaskPromise::get_return_object() noexcept
{
	LOG_TRACE("task promise get_return_object");
    return Task{m_controller};
}
