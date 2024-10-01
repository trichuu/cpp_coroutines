#pragma once
#include <chrono>
#include <coroutine>
#include <deque>
#include <queue>
#include <thread>

namespace cocos {
using TimePoint = std::chrono::time_point<std::chrono::steady_clock>;
using Duration = std::chrono::duration<long long>;

template <typename T> class Task;

struct Delay {
  std::coroutine_handle<> sleeping_coro;
  std::chrono::time_point<std::chrono::steady_clock> awake_time;
  bool operator<(const Delay &other) const {
    return awake_time > other.awake_time;
  }
};

class EventLoop {
  using Coro = std::coroutine_handle<>;
  std::deque<Coro> tasks;
  std::priority_queue<Delay> delays;

public:
  void add_task(Coro handle) { tasks.push_back(handle); }
  void
  add_delayed_task(Coro handle,
                   std::chrono::time_point<std::chrono::steady_clock> delay) {
    delays.push({handle, delay});
  }
  void run() {
    while (!tasks.empty() || !delays.empty()) {
      if (!tasks.empty()) {
        auto task = tasks.front();
        tasks.pop_front();
        task.resume();
        continue;
      }
      if (!delays.empty()) {
        auto delay = delays.top();
        if (delay.awake_time <= std::chrono::steady_clock::now()) {
          delays.pop();
          delay.sleeping_coro.resume();
        } else {
          std::this_thread::sleep_until(delay.awake_time);
        }
      }
    }
  }
  static EventLoop &get_loop() {
    static EventLoop instance;
    return instance;
  }
};
} // namespace cocos