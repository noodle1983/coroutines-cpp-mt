#ifndef PROCESSOR_H
#define PROCESSOR_H

#include "Singleton.hpp"
#include "worker_group.h"
#include "worker.h"

namespace nd{

};

#define NEW_JOB(...) (new nd::Job(std::bind(__VA_ARGS__)))
#define PROCESS(...) process(NEW_JOB(__VA_ARGS__))

#define g_processor nd::Singleton<nd::CppWorker, 0>::instance()


#endif /* PROCESSOR_H */

