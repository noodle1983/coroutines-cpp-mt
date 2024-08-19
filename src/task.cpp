#include "Task.h"

using namespace nd;

Task TaskPromise::get_return_object() noexcept
{
    return Task{std::coroutine_handle<TaskPromise>::from_promise(*this)};
}
