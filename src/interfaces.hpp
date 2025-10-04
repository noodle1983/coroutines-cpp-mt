#pragma once

#include "src_id.h"

#include <atomic>

namespace nd {

class IWaiter;
struct Empty {};
template <bool exists, class T>
using Maybe = std::conditional_t<exists, T, Empty>;

template <typename ReturnType>
class BaseTask;

template <typename T>
class ID {
public:
    ID() : m_id(++s_id) {}
    operator uint64_t() const { return m_id; }
    uint64_t Id() const { return m_id; }

private:
    static std::atomic<uint64_t> s_id;
    uint64_t m_id;
};
template <typename T>
std::atomic<uint64_t> ID<T>::s_id = 0;

class ITask {
public:
    ITask() : m_stat_name(nullptr) {}
    virtual ~ITask() {}

    uint64_t Id() const { return m_id.Id(); }
    void SetStatInfo(const char* _stat_name, const std::source_location& _loc) {
        m_src_id.Set(_loc);
        m_stat_name = _stat_name;
    }

    void GetStatId(char* _buff, size_t _buff_len) const {
        if (m_stat_name) {
            snprintf(_buff, _buff_len - 1, "%s", m_stat_name);
        } else {
            m_src_id.Get(_buff, _buff_len);
        }
    }

    virtual void Resume(IWaiter* waiter, uint32_t resume_key) = 0;
    virtual uint32_t GetResumeKey(IWaiter* waiter) = 0;

    friend std::ostream& operator<<(std::ostream& os, const ITask& obj) {
        char buff[128] = {0};
        obj.GetStatId(buff, sizeof(buff));
        os << "task-" << obj.m_id << '[' << buff << ']';
		return os;
	}

protected:
    ID<BaseTask<Empty>> m_id;

    nd::SrcId m_src_id;
    const char* m_stat_name;
};

class IWaiter {
public:
    IWaiter() {}
    virtual ~IWaiter() {}

protected:
};

}  // namespace nd
