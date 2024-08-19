#ifndef IPROCESSOR_H
#define IPROCESSOR_H

#include "singleton.hpp"
#include "mylist.h"

namespace nd{

    using String = std::string;

    class IJob {
    public:
        IJob() {
            INIT_LIST_HEAD(&entry);
        }
        virtual ~IJob() {
			list_del_init(&entry);
        }
        virtual void before_exec() = 0;
        virtual void exec() = 0;
        virtual void after_exec() = 0;
        virtual void on_error(const String& err) = 0;

        list_head entry;
    };

    using JobPtr = IJob*;
    using TimerHandle = min_heap_item_t*;
    using TimerCallback = void (*)(void *arg);

    class IProcessor {
    public:
        virtual ~IProcessor() = 0;
        virtual void process(uint64_t worker_id, JobPtr job) = 0;
        virtual void process(JobPtr job) { process(0, job); } // simple method for the processor with only 1 worker
        virtual TimerHandle addLocalTimer(uint64_t ms_time, TimerCallback callback, void* arg) = 0;
        virtual void cancelLocalTimer(TimerCallback& handle) = 0;
    };

    class IWorker {
    public:
        virtual ~IWorker() = 0;
        virtual void process(JobPtr job) = 0;
        virtual TimerHandle addLocalTimer(uint64_t ms_time, TimerCallback callback, void* arg) = 0;
        virtual void cancelLocalTimer(TimerHandle& handle) = 0;
        virtual void thread_main() = 0;
        virtual void stop() = 0;
        virtual void waitStop() = 0;

        virtual bool isJobQueueEmpty() = 0;
        virtual size_t getQueueSize() = 0;
    };

};

#define NEW_JOB(...) (new nd::Job(std::bind(__VA_ARGS__)))
#define PROCESS(...) process(NEW_JOB(__VA_ARGS__))

#define g_processor nd::Singleton<nd::CppWorker, 0>::instance()


#endif /* PROCESSOR_H */

