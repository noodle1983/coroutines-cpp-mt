#pragma once

#include <coroutine>

#include "worker.hpp"
#include "waiter.hpp"

namespace nd {

// suspend coroutine for a specified time in millisecond
// run in the same thread with the coroutine
class TimeWrappedWaiter {
public:
    using WaiterType = TaskInnerWaiter<TimeWrappedWaiter>;
    using RetType = void;

    TimeWrappedWaiter(WaiterType* _waiter, uint64_t _millisecond)
        : m_waiter(_waiter)
        , m_mstime(_millisecond) {
        m_timer_handle = m_mstime <= 0 ? nullptr : Worker::GetCurrentWorker()->AddLocalTimer(m_mstime, [this]() {
            m_timer_handle = nullptr;
            if (m_waiter) { m_waiter->Resume(); }
        });

    }
    virtual ~TimeWrappedWaiter() { Reset(); }

    TimeWrappedWaiter& Reset(uint64_t _time = 0) {
        if (m_timer_handle != nullptr) { Worker::GetCurrentWorker()->CancelLocalTimer(m_timer_handle); }
        if (_time > 0) { m_mstime = _time; }
        return *this;
    }

    // NOLINTNEXTLINE
    bool AwaitReady() noexcept {
        if (m_mstime == 0 || (m_timer_handle == nullptr)) { return true; }
        return false;
    }

    // NOLINTNEXTLINE
    void AwaitSuspend() noexcept { }
    
    // call Resume() to resume the suspended coroutine

    // NOLINTNEXTLINE
    //RetType GetRetObj() {}
private:
    WaiterType* m_waiter;
    uint64_t m_mstime;
    TimerHandle m_timer_handle;
};
using TimeWaiter = TimeWrappedWaiter::WaiterType;

}  // namespace nd

// can't get the right location for this usage
// inline nd::TimeWrappedWaiter::WaiterType operator"" _ms(unsigned long long milliseconds) {
//     return nd::TimeWrappedWaiter::WaiterType(static_cast<uint64_t>(milliseconds));
// }
// 
// inline nd::TimeWrappedWaiter::WaiterType operator"" _s(unsigned long long seconds) {
//     return nd::TimeWrappedWaiter::WaiterType(static_cast<uint64_t>(seconds * 1000));
// }

