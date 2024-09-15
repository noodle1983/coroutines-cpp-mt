#include <cstddef>
#include <string>

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
    LOG_TRACE("\tsizeof std::list<CoroutineController<void>::WaitingTask> = "
              << sizeof(std::list<nd::CoroutineController<void>::WaitingTask>));
    LOG_TRACE("\tsizeof std::mutex = " << sizeof(std::mutex));
    LOG_TRACE("\tsizeof std::coroutine_handle<> = " << sizeof(std::coroutine_handle<>));
    LOG_TRACE("----------------------------------------");
    LOG_TRACE("sizeof nd::CoroutineController<char> = " << sizeof(nd::CoroutineController<char>));
    LOG_TRACE("\tsizeof nd::ID<CoroutineController<char>> = " << sizeof(nd::ID<nd::CoroutineController<char>>));
    LOG_TRACE("\tsizeof std::list<CoroutineController<char>::WaitingTask> = "
              << sizeof(std::list<nd::CoroutineController<char>::WaitingTask>));
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

        LOG_TRACE("------------------------------------> coroutinue 2");
        {
            // bg_task start from here
            auto bg_task = []() -> nd::Task<> {
                LOG_TRACE("-> bg task in worker" << nd::Worker::GetCurrWorkerName());
                co_await nd::TimeWaiter(1000);  // NOLINT
                LOG_TRACE("<- bg task in worker" << nd::Worker::GetCurrWorkerName() << " after 1 sec later");
                co_return;
            }();
            co_await bg_task.RunOnProcessor(WorkerGroup::BG1);

            // bg_task destroys here.
            // all objects must be destroyed, as shown in the log.
        }
        LOG_TRACE("<------------------------------------ coroutinue 2");

        LOG_TRACE("------------------------------------> coroutinue 3");
        {
            // another bg_task start from here
            auto bg_task = []() -> nd::Task<> {
                LOG_TRACE("-> bg task in worker" << nd::Worker::GetCurrWorkerName());
                co_await nd::TimeWaiter(2000);  // NOLINT
                LOG_TRACE("<- bg task in worker" << nd::Worker::GetCurrWorkerName() << " after 2 secs later");
                co_return;
            }();
            co_await bg_task.RunOnProcessor(WorkerGroup::BG2);
            // bg_task destroys here.
            // all objects must be destroyed, as shown in the log.
        }
        LOG_TRACE("<------------------------------------ coroutinue 3");

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

        LOG_TRACE("------------------------------------> mixed coroutinue 5 and 6");
        {
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

            // bg_task2's coroutine handle is compleated and destroyed first, bg_task2 remains
            co_await bg_task1;
            co_await bg_task2;
        }
        FLOG_TRACE("<------------------------------------ mixed coroutinue 5 and 6");

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

TEST_F(CoroutinesCppMtTest, ReturnValueFromTask) {
    auto main_task = []() -> nd::Task<> {
        const char* str = "hello world!";
        FLOG_TRACE("-> main task in worker:0x%lx-%s", (intptr_t)str, str);

        try {
            auto bg_task = [str]() -> nd::Task<std::string> {
                FLOG_TRACE("-> bg task return 0x%lx[%s]", (intptr_t)str, str);
                co_return std::string{str};
            }();
            std::string result = co_await bg_task.RunOnProcessor(WorkerGroup::BG1);
            EXPECT_EQ(result, str);
        } catch (const std::exception& e) { LOG_TRACE("exception caught: " << e.what()); }

        std::string strstr = std::string(str);
        LOG_TRACE("-> main task in worker:" << strstr);
        try {
            auto bg_task = [strstr, str]() -> nd::Task<std::string> {
                EXPECT_EQ(strstr, str);
                LOG_TRACE("-> bg task in worker:" << strstr);
                co_return strstr;
            }();
            std::string result = co_await bg_task.RunOnProcessor(WorkerGroup::BG1);
            EXPECT_EQ(result, str);
        } catch (const std::exception& e) { LOG_TRACE("exception caught: " << e.what()); }

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

TEST_F(CoroutinesCppMtTest, HandleExceptionFromTask) {
    auto main_task = []() -> nd::Task<> {
        LOG_TRACE("-> main task in worker" << nd::Worker::GetCurrWorkerName());
        try {
            auto bg_task = []() -> nd::Task<> {
                LOG_TRACE("-> bg task in worker" << nd::Worker::GetCurrWorkerName());
                throw std::runtime_error("runtime error");
                LOG_TRACE("-> bg task in worker" << nd::Worker::GetCurrWorkerName());
                co_return;
            }();
            co_await bg_task.RunOnProcessor(WorkerGroup::BG1);
        } catch (const std::exception& e) { LOG_TRACE("exception caught: " << e.what()); }

        co_await nd::TimeWaiter(1);  // NOLINT

        FLOG_TRACE("<- main task in worker %s", nd::Worker::GetCurrWorkerName());
    }();

    main_task.RunOnProcessor();  // run on current worker, which is main worker on
                                 // the marked main thread.
    main_task.WaitInMain();
    nd::Worker::GetMainWorker()->WaitUntilEmpty();
}
