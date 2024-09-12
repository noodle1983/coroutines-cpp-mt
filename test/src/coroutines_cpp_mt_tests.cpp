#include <cstddef>

#include "gtest/gtest.h"
#include "log.hpp"
#include "task.hpp"
#include "time_waiter.hpp"
#include "worker_manager.hpp"

using namespace std;

// usage 1 : define worker group(thread model)
// NOLINTNEXTLINE
namespace WorkerGroup {
enum {
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
        nd::Worker::MarkMainThread();
        g_worker_mgr->Init(WorkerGroup::MAX);
        g_worker_mgr->Start(WorkerGroup::BG1, 1, "bg1");
        g_worker_mgr->Start(WorkerGroup::BG2, 1, "bg2");
        LOG_DEBUG("worker inited!");
    };

    static void TearDownTestCase() {};
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(CoroutinesCppMtTest, TypeSize) {
    LOG_TRACE("----------------------------------------");
    LOG_TRACE("sizeof nd::CoroutineController<void> = " << sizeof(nd::CoroutineController<void>));
    LOG_TRACE("\tsizeof nd::ID<CoroutineController<void>> = " << sizeof(nd::ID<nd::CoroutineController<void>>));
    LOG_TRACE("\tsizeof std::list<CoroutineController<void>::WaitingTask> = " << sizeof(std::list<nd::CoroutineController<void>::WaitingTask>));
    LOG_TRACE("\tsizeof std::mutex = " << sizeof(std::mutex));
    LOG_TRACE("\tsizeof std::coroutine_handle<> = " << sizeof(std::coroutine_handle<>));
    LOG_TRACE("----------------------------------------");
    LOG_TRACE("sizeof nd::CoroutineController<char> = " << sizeof(nd::CoroutineController<char>));
    LOG_TRACE("\tsizeof nd::ID<CoroutineController<char>> = " << sizeof(nd::ID<nd::CoroutineController<char>>));
    LOG_TRACE("\tsizeof std::list<CoroutineController<char>::WaitingTask> = " << sizeof(std::list<nd::CoroutineController<char>::WaitingTask>));
    LOG_TRACE("\tsizeof std::mutex = " << sizeof(std::mutex));
    LOG_TRACE("\tsizeof std::coroutine_handle<> = " << sizeof(std::coroutine_handle<>));
    LOG_TRACE("\tsizeof char = " << sizeof(char));
    LOG_TRACE("----------------------------------------");
    EXPECT_EQ(sizeof(nd::CoroutineController<void>) + sizeof(size_t), sizeof(nd::CoroutineController<char>));
    EXPECT_EQ(sizeof(nd::CoroutineController<void>) + sizeof(size_t), sizeof(nd::CoroutineController<size_t>));
}

TEST_F(CoroutinesCppMtTest, Wait_Bg_Task_On_Main_Thread) {
    auto main_task = []() -> nd::Task<> {
        LOG_TRACE("-> main task in worker" << nd::Worker::GetCurrWorkerName());
        LOG_TRACE("----------------------------------------");

        co_await []() -> nd::Task<> {
            LOG_TRACE("-> bg task in worker" << nd::Worker::GetCurrWorkerName());
            co_await nd::TimeWaiter(1000);  // NOLINT
            LOG_TRACE("<- bg task in worker" << nd::Worker::GetCurrWorkerName() << " after 1 sec later");
            co_return;
        }()
                             .RunOnProcessor(WorkerGroup::BG1);  // NOLINT, clang-format insists on this indent...

        LOG_TRACE("----------------------------------------");

        co_await []() -> nd::Task<> {
            LOG_TRACE("-> bg task in worker" << nd::Worker::GetCurrWorkerName());
            co_await nd::TimeWaiter(2000);  // NOLINT
            LOG_TRACE("<- bg task in worker" << nd::Worker::GetCurrWorkerName() << " after 2 secs later");
            co_return;
        }()
                             .RunOnProcessor(WorkerGroup::BG2);  // NOLINT

        LOG_TRACE("----------------------------------------");

        // you should see the destruction log of upper promise and task
        // main's resume is called in the BG1 thread, it can be run before the
        // destruction of upper promise and task. so wait for a while to see the log
        co_await nd::TimeWaiter(1);  // NOLINT

        LOG_TRACE("<- main task in worker" << nd::Worker::GetCurrWorkerName());
    }();

    main_task.RunOnProcessor();  // run on current worker, which is main worker on
                                 // the marked main thread.
    main_task.WaitInMain();
    nd::Worker::GetMainWorker()->WaitUntilEmpty();
}

TEST_F(CoroutinesCppMtTest, Run_Multi_Task_On_Same_Thread) {
    auto main_task = []() -> nd::Task<> {
        LOG_TRACE("-> main task in worker" << nd::Worker::GetCurrWorkerName());
        LOG_TRACE("----------------------------------------");

        auto bg_task1 = []() -> nd::Task<> {
            LOG_TRACE("-> bg task in worker" << nd::Worker::GetCurrWorkerName());
            co_await nd::TimeWaiter(1500);  // NOLINT
            LOG_TRACE("<- bg task in worker" << nd::Worker::GetCurrWorkerName() << " after 1.5 sec later");
            co_return;
        }();
        bg_task1.RunOnProcessor(WorkerGroup::BG1);

        auto bg_task2 = []() -> nd::Task<> {
            LOG_TRACE("-> bg task in worker" << nd::Worker::GetCurrWorkerName());
            co_await nd::TimeWaiter(1000);  // NOLINT
            FLOG_TRACE("<- bg task in worker %s after 1 sec later", nd::Worker::GetCurrWorkerName());
            co_return;
        }();
        bg_task2.RunOnProcessor(WorkerGroup::BG1);

        co_await bg_task1;
        co_await bg_task2;

        FLOG_TRACE("----------------------------------------");
        // you should see the destruction log of upper promise and task
        // main's resume is called in the BG1 thread, it can be run before the
        // destruction of upper promise and task. so wait for a while to see the log
        co_await nd::TimeWaiter(1);  // NOLINT

        FLOG_TRACE("<- main task in worker %s", nd::Worker::GetCurrWorkerName());
    }();

    main_task.RunOnProcessor();  // run on current worker, which is main worker on
                                 // the marked main thread.
    main_task.WaitInMain();
    nd::Worker::GetMainWorker()->WaitUntilEmpty();
}

TEST_F(CoroutinesCppMtTest, Long_DivideBy_Long) {}
