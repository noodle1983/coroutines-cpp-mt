#pragma once


#include "waiter.hpp"

namespace nd {

class SeqGennerWrappedWaiter {
public:
    using WaiterType = TaskInnerWaiter<SeqGennerWrappedWaiter>;
    using RetType = int;

    SeqGennerWrappedWaiter(WaiterType* _waiter, int _start = 0, int _step = 1) 
        : m_waiter(_waiter) 
        , m_curr(_start)
        , m_start(_start)
        , m_step(_step)
    {
    }
    virtual ~SeqGennerWrappedWaiter() { Reset(); }

    SeqGennerWrappedWaiter& Reset() {
        m_curr = m_start;
        return *this;
    }

    // NOLINTNEXTLINE
    bool AwaitReady() noexcept {
        return true;
    }

    // NOLINTNEXTLINE
    void AwaitSuspend() noexcept {}

    // call Resume() to resume the suspended coroutine

    // NOLINTNEXTLINE
    RetType GetRetObj() { 
        int ret = m_curr;
        m_curr += m_step;
        return ret;
    }
private:
    WaiterType* m_waiter;

    int m_curr;
    int m_start;
    int m_step;
};
using SeqGennerWaiter = SeqGennerWrappedWaiter::WaiterType;

}  // namespace nd

