#include "task.h"
#include "worker_manager.h"
#include "log.h"

#include "gtest/gtest.h"

using namespace std;

namespace WorkerGroup{
	enum{
        BG = 0,

        MAX,
	};
};


class CoroutinesCppMtTest : public ::testing::Test {
protected:
  static void SetUpTestCase() {
	nd::Worker::markMainThread();
    g_worker_mgr->init(WorkerGroup::MAX);
    g_worker_mgr->start(WorkerGroup::BG, 1, "bg");
	LOG_DEBUG("worker inited!");
  };

  static void TearDownTestCase() {
  };
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(CoroutinesCppMtTest, Wait_Task_On_Main_Thread) {
    auto helloTask = []()->nd::Task{
		LOG_DEBUG("hello in thread id:0x" << std::hex << this_thread::get_id());
        co_return;
	}();

    helloTask.runOnProcessor(WorkerGroup::BG);
    LOG_DEBUG("main1 in thread id:0x" << std::hex << this_thread::get_id());
    helloTask.waitInMain();
    LOG_DEBUG("main2 in thread id:0x" << std::hex << this_thread::get_id());
    nd::Worker::getMainWorker()->waitUntilEmpty();
}

TEST_F(CoroutinesCppMtTest, 9_DivideBy_3) {
}

TEST_F(CoroutinesCppMtTest, 17_DivideBy_19) {
}

TEST_F(CoroutinesCppMtTest, Long_DivideBy_Long) {
}

