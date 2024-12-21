#ifndef COROS_WHENALL
#define COROS_WHENALL
#include "eventloop.hpp"
#include "task.hpp"
#include <coroutine>
namespace coros {
template <typename T> struct WaitFinishedAwaiter {
  Task<T> *task;

  bool await_ready() { return task->raw_handle().done(); }
  std::coroutine_handle<> await_suspend(std::coroutine_handle<> h) {
    this->task->raw_handle().prev_hdl = h;
    return std::noop_coroutine();
  }
  void await_transform() {}
};
template <typename Iter> Task<void> when_all(Iter begin, Iter end) {
  for (auto it{begin}; it != end; ++it) {
    EventLoop::get_loop().add_task(*it);
  }
  for (auto it{begin}; it != end; ++it) {
    co_await WaitFinishedAwaiter{std::addressof(*it)};
  }
}
} // namespace coros

#endif