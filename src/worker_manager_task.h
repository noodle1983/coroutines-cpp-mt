#ifndef WORKER_MANAGER_TASK_H
#define WORKER_MANAGER_TASK_H

#include <log.hpp>
#include <string.h>

#include "task.hpp"
#include "worker_manager.hpp"

namespace nd {

using TaskFunction = std::function<nd::Task<>()>;
class WorkerManagerTask {
public:
    static Task<> RunOnAllWorkers(TaskFunction _func, int _except_group_id = PreDefWorkerGroup::Invalid);
    static Task<> RunOnGroupWorkers(TaskFunction _func, int _group_id, int _except_group_id = PreDefWorkerGroup::Invalid);
};
};  // namespace nd

#endif /* WORKER_MANAGER_TASK_H */
