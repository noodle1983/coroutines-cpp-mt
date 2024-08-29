#ifndef PROCESSORTYPES_H
#define PROCESSORTYPES_H

#include <stdint.h>
#include <list>
#include <functional>

namespace nd{
    using ProcessWorkerId = int32_t;
    using SessionId = uint64_t;
    typedef std::function<void ()> Job;
    typedef std::list<Job*> JobQueue;

    namespace PreDefWorkerGroup{
        enum{
            Main = -1,
            Current = -2,
            Invalid = -3,
        };
    };
};

#endif /* PROCESSORTYPES_H */

