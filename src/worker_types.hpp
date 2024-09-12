#ifndef WORKER_TYPES_H
#define WORKER_TYPES_H

#include <stdint.h>

#include <functional>
#include <list>

namespace nd {
using ProcessWorkerId = int32_t;
using SessionId = uint64_t;
using Job = std::function<void()>;
using JobQueue = std::list<Job*>;

namespace PreDefWorkerGroup {  // NOLINT
enum {
    Main = -1,
    Current = -2,
    Invalid = -3,
};
}
}  // namespace nd

#endif /* WORKER_TYPES_H */
