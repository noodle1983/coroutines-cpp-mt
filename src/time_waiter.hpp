#pragma once

#include <coroutine>

#include "worker.hpp"

namespace nd {

// suspend coroutine for a specified time in millisecond
// run in the same thread with the coroutine
class TimeWaiter {
public:
    TimeWaiter(uint64_t _millisecond) : m_mstime(_millisecond), m_timer_handle(nullptr) {}
    virtual ~TimeWaiter() { Reset(); }

    TimeWaiter& Reset(uint64_t _time = 0) {
        assert(!m_coroutine);
        if (m_timer_handle != nullptr) {
            Worker::GetCurrentWorker()->CancelLocalTimer(m_timer_handle);
        }
        if (_time > 0) {
            m_mstime = _time;
        }
        return *this;
    }

    // NOLINTNEXTLINE
    bool await_ready() noexcept {
        if (m_mstime == 0 || (m_timer_handle != nullptr)) {
            return true;
        }

        m_timer_handle = Worker::GetCurrentWorker()->AddLocalTimer(m_mstime, [this]() {
            m_timer_handle = nullptr;
            if (m_coroutine) {
                auto coroutine = m_coroutine;
                m_coroutine = nullptr;
                coroutine.resume();
            }
        });
        return false;
    }

    // NOLINTNEXTLINE
    void await_suspend(std::coroutine_handle<> _awaiting_coroutine) noexcept { m_coroutine = _awaiting_coroutine; }

    // NOLINTNEXTLINE
    void await_resume() const noexcept {}

private:
    uint64_t m_mstime;
    TimerHandle m_timer_handle;
    std::coroutine_handle<> m_coroutine;
};
}  // namespace nd
