#include "task.h"
#include "worker_manager.h"
#include "TimeWaiter.h"
#include "log.h"

#include "gtest/gtest.h"

using namespace std;

// usage 1 : define worker group(thread model)
namespace WorkerGroup{
	enum{
        BG1 = 0,
        BG2 = 1,
        //...

        MAX,
	};
};


class CoroutinesCppMtTest : public ::testing::Test {
protected:
  static void SetUpTestCase() {

	// usage 2 : start thread
	nd::Worker::markMainThread();
    g_worker_mgr->init(WorkerGroup::MAX);
    g_worker_mgr->start(WorkerGroup::BG1, 1, "bg1");
    g_worker_mgr->start(WorkerGroup::BG2, 1, "bg2");
	LOG_DEBUG("worker inited!");
  };

  static void TearDownTestCase() {
  };
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(CoroutinesCppMtTest, Wait_Bg_Task_On_Main_Thread) {
    auto mainTask = []()->nd::Task{
		LOG_TRACE("-> main task in thread id:0x" << std::hex << this_thread::get_id() << std::dec);
        auto bgTask = []()->nd::Task {
            LOG_TRACE("-> bg task in thread id:0x" << std::hex << this_thread::get_id() << std::dec);
            co_await nd::TimeWaiter(1000);
            LOG_TRACE("<- bg task in thread id:0x" << std::hex << this_thread::get_id() << std::dec << " after 1 sec later");
            co_return;
		}();
        co_await bgTask.runOnProcessor(WorkerGroup::BG1);
		LOG_TRACE("<- main task in thread id:0x" << std::hex << this_thread::get_id() << std::dec);
	}();

    mainTask.runOnProcessor(); // run on current worker, which is main worker on the marked main thread.
    mainTask.waitInMain();
    nd::Worker::getMainWorker()->waitUntilEmpty();
}

TEST_F(CoroutinesCppMtTest, Wait_Bg_Task_On_Bg_Thread) {
}

TEST_F(CoroutinesCppMtTest, 17_DivideBy_19) {
}

TEST_F(CoroutinesCppMtTest, Long_DivideBy_Long) {
}

