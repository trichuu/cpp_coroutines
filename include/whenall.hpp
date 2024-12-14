#ifndef COCOS_WHENALL
#define COCOS_WHENALL
#include "eventloop.hpp"
#include "task.hpp"
#include <coroutine>
namespace cocos {
struct CheckAwaiter {
  std::coroutine_handle<> handle;
  bool await_ready() { return handle.done(); }
  std::coroutine_handle<> await_suspend(std::coroutine_handle<> h) {
    EventLoop::get_loop().add_task(h);
    return std::noop_coroutine();
  }
  void await_transform() {}
};
template <typename Iter> Task<void> when_all(Iter begin, Iter end) {
  for (auto it{begin}; it != end; ++it) {
    EventLoop::get_loop().add_task(*it);
  }
  for (auto it{begin}; it != end; ++it) {
    co_await CheckAwaiter{(*it).raw_handle()};
  }
}
} // namespace cocos

#endif