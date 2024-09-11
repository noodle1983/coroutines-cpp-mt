#pragma once

#include "worker.hpp"
#include <coroutine>

namespace nd {

	// suspend coroutine for a specified time in millisecond
	// run in the same thread with the coroutine
	class TimeWaiter {
	public:
		TimeWaiter(uint64_t millisecond) 
			: m_mstime(millisecond) 
			, m_timerHandle(nullptr)
		{}
		virtual ~TimeWaiter() {
			reset();
		}

		TimeWaiter& reset(uint64_t time = 0) {
			assert(!m_coroutine);
			if (m_timerHandle) {
				Worker::getCurrentWorker()->cancelLocalTimer(m_timerHandle);
			}
			if (time > 0) {
				m_mstime = time;
			}
			return *this;
		}

        bool await_ready() noexcept { 
			if (m_mstime == 0 || m_timerHandle) {
				return true;
			}

			m_timerHandle = Worker::getCurrentWorker()->addLocalTimer(m_mstime, [this]() {
				m_timerHandle = nullptr;
				if (m_coroutine) {
					auto coroutine = m_coroutine;
					m_coroutine = nullptr;
					coroutine.resume();
				}
			});
			return false;
		}

        void await_suspend(std::coroutine_handle<> awaitingCoroutine) noexcept {
            m_coroutine = awaitingCoroutine;
        }
		void await_resume() const noexcept { }



	private:
		uint64_t m_mstime;
		TimerHandle m_timerHandle;
		std::coroutine_handle<> m_coroutine;
	};
}
