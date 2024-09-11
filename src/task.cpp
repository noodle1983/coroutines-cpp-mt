#include "task.hpp"

using namespace nd;

void CoroutineController::AddWaitingTask(BaseTask* _task, Worker* _worker) {
  if (IsDone()) {
    _worker->AddJob(new nd::Job{[_task]() { _task->OnCoroutineReturn(); }});
    return;
  }
  std::lock_guard<std::mutex> lock(m_waiting_tasks_mutex);
  if (m_coroutine == nullptr) {
    _worker->AddJob(new nd::Job{[_task]() { _task->OnCoroutineReturn(); }});
    return;
  }
  m_waiting_tasks.emplace_back(_task, _worker);
}

void CoroutineController::OnCoroutineReturn() {
  SaveResult();

  {
    // task is waited in other coroutine, so it ought to be exist
    std::lock_guard<std::mutex> lock(m_waiting_tasks_mutex);
    for (auto& waiting_task : m_waiting_tasks) {
      auto* task = std::get<0>(waiting_task);
      auto* worker = std::get<1>(waiting_task);
      worker->AddJob(
        new nd::Job{[task]() { task->OnCoroutineReturn(); }});
    }
    m_waiting_tasks.clear();
  }
}

void CoroutineController::OnCoroutineDone() {
  std::lock_guard<std::mutex> lock(m_waiting_tasks_mutex);
  if (m_coroutine) {
    //m_coroutine.destroy();
    m_coroutine = nullptr;
  }
}

Task TaskPromise::get_return_object() noexcept
{
  LOG_TRACE("promise-" << m_id << " get_return_object");
  return Task{m_controller};
}
