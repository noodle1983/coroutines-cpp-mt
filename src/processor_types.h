#ifndef PROCESSORTYPES_H
#define PROCESSORTYPES_H

#include <stdint.h>
#include <list>
#include <functional>

namespace nd{
    using ProcessorGroup = int32_t;
    using ProcessWorkerId = int32_t;
    using SessionId = uint64_t;
    typedef std::function<void ()> Job;
    typedef std::list<Job*> JobQueue;

    namespace PreDefProcessGroup{
        enum{
            Main = -1,
            CurrentWorker = -2,
            Invalid = -3,
        };
    };
};

#endif /* PROCESSORTYPES_H */

