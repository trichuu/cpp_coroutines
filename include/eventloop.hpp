#ifndef COCOS_EVENTLOOP
#define COCOS_EVENTLOOP
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
  /**
   * @brief To ajust the priority queue to be a min heap.
   */
  bool operator<(const Delay &other) const {
    return awake_time > other.awake_time;
  }
};

class EventLoop {
  using Coro = std::coroutine_handle<>;
  std::deque<Coro> tasks;
  std::priority_queue<Delay> delays;

public:
  /**
   * @brief Add a coroutine to be resumed.
   * @param handle The coroutine handle representing the coroutine.
   */
  void add_task(Coro handle) { tasks.push_back(handle); }
  /**
   * @brief Add a task to be runned.
   * @param task The task to be added.
   */
  template <typename T> void add_task(const Task<T> &task) {
    tasks.push_back(task.co_hdl);
  }
  /**
   * @brief Delay a resuming of a coroutine, until the awake time.
   *
   * @param handle The delayed coroutine.
   * @param delay The awake time.
   */
  void
  add_delayed_task(Coro handle,
                   std::chrono::time_point<std::chrono::steady_clock> delay) {
    delays.push({handle, delay});
  }
  /**
   * @brief Run the event loop.
   *
   */
  void run() {
    while (!tasks.empty() || !delays.empty()) {
      if (!tasks.empty()) {
        auto task = tasks.front();
        tasks.pop_front();
        task.resume();
        continue;
      } else if (!delays.empty()) {
        auto delay = delays.top();
        if (delay.awake_time > std::chrono::steady_clock::now()) {
          std::this_thread::sleep_until(delay.awake_time);
        }
        delays.pop();
        delay.sleeping_coro.resume();
        continue;
      }
    }
  }
  /**
   * @brief Get the singleton loop object.
   *
   * @return EventLoop& The global EventLoop object.
   */
  static EventLoop &get_loop() {
    static EventLoop instance;
    return instance;
  }
};
} // namespace cocos
#endif // COCOS_EVENTLOOP