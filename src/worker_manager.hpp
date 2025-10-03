#ifndef WORKER_MANAGER_H
#define WORKER_MANAGER_H

#include <log.hpp>
#include <string.h>

#include "singleton.hpp"
#include "worker.hpp"
#include "worker_group.hpp"

namespace nd {
class WorkerManager {
    enum { MaxWorkerGroup = 10 };

public:
    WorkerManager() : m_max_worker_group(0), m_worker_groups(nullptr) {}

    ~WorkerManager() { StopAll(); }

    void Init(unsigned _max_worker_group) {
        MY_ASSERT(_max_worker_group <= MaxWorkerGroup, "invalid max_worker_group:%d > MaxWorkerGroup.", _max_worker_group);
        m_max_worker_group = _max_worker_group;

        m_worker_groups = new WorkerGroup*[m_max_worker_group];
        memset(m_worker_groups, 0, sizeof(WorkerGroup*) * m_max_worker_group);
    }

    void Start(unsigned _worker_group_id, signed _processor_num, const std::string& _the_name = "xxx") {
        MY_ASSERT(_worker_group_id < m_max_worker_group, "invalid group id:%d >= MaxWorkerGroup", _worker_group_id);
        MY_ASSERT(m_worker_groups[_worker_group_id] == nullptr, "worker group is started twice");

        m_worker_groups[_worker_group_id] = new WorkerGroup(_worker_group_id, _processor_num, _the_name);
        m_worker_groups[_worker_group_id]->Start();
    }

    void Stop(unsigned _worker_group_id) {
        MY_ASSERT(_worker_group_id < m_max_worker_group, "invalid group id:%d >= MaxWorkerGroup", _worker_group_id);
        if (m_worker_groups[_worker_group_id] == nullptr) { return; }

        m_worker_groups[_worker_group_id]->WaitStop();
        delete m_worker_groups[_worker_group_id];
        m_worker_groups[_worker_group_id] = nullptr;
    }

    void StopAll() {
        if (m_max_worker_group == 0) { return; }
        if (m_worker_groups == nullptr) { return; }

        for (unsigned i = 0; i < m_max_worker_group; ++i) {
            if (m_worker_groups[i] == nullptr) { continue; }

            Stop(i);
        }
        delete[] m_worker_groups;
        m_worker_groups = nullptr;
    }

    Worker* GetWorker(unsigned _worker_group_id, size_t _session_id) {
        if (_worker_group_id == PreDefWorkerGroup::Main) { return Worker::GetMainWorker(); }
        if (_worker_group_id == PreDefWorkerGroup::Current) { return Worker::GetCurrentWorker(); }

        MY_ASSERT(0 <= _worker_group_id && _worker_group_id < m_max_worker_group, "invalid worker group:%d", _worker_group_id);
        MY_ASSERT(m_worker_groups[_worker_group_id] != nullptr, "worker group is not exist!");

        return m_worker_groups[_worker_group_id]->GetWorker(_session_id);
    }

    void RunOnWorkerGroup(int _worker_group_id, size_t _session_id, Job* _job) {
        if (_worker_group_id == PreDefWorkerGroup::Main) { return RunOnMainThread(_job); }
        if (_worker_group_id == PreDefWorkerGroup::Current) { return RunOnCurrentThread(_job); }

        MY_ASSERT(0 <= _worker_group_id && _worker_group_id < m_max_worker_group, "invalid worker group id:%d", _worker_group_id);
        MY_ASSERT(m_worker_groups[_worker_group_id] != nullptr, "target worker group:%d is not exist", _worker_group_id);
        m_worker_groups[_worker_group_id]->AddJob(_session_id, _job);
    }

    static void RunOnMainThread(Job* _job) { Worker::GetMainWorker()->AddJob(_job); }

    static void RunOnCurrentThread(Job* _job) { Worker::GetCurrentWorker()->AddJob(_job); }

protected:
    size_t m_max_worker_group;
    WorkerGroup** m_worker_groups;
};
};  // namespace nd

#define g_worker_mgr (nd::Singleton<nd::WorkerManager, 0>::Instance())

#endif /* WORKER_MANAGER_H */
