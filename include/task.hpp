#ifndef COCOS_TASK
#define COCOS_TASK
#include "coroutine_concepts.hpp"
#include "eventloop.hpp"
#include <algorithm>
#include <coroutine>
#include <exception>
#include <type_traits>
#include <utility>
#include <variant>

namespace cocos {
template <typename T = void> struct TaskPromise;
template <typename T = void> class Task;
template <typename T> struct TaskAwaiter;

struct FinalAwaiter {
  std::coroutine_handle<> prev_hdl;
  bool await_ready() const noexcept { return false; }
  std::coroutine_handle<> await_suspend(std::coroutine_handle<>) noexcept {
    if (this->prev_hdl) {
      return this->prev_hdl;
    }
    return std::noop_coroutine();
  }
  void await_resume() const noexcept {}
};

template <typename T> struct TaskAwaiter {
  Task<T> task;
  /**
   * @brief Since the task is lazy, it is never ready when it is first awaited.
   */
  bool await_ready() const noexcept { return false; }
  /**
   * @brief Add the awaiting coroutine to the event loop.
   */
  std::coroutine_handle<> await_suspend(std::coroutine_handle<>) noexcept {
    EventLoop::get_loop().add_task(task);
    return std::noop_coroutine();
  }
  /**
   * @brief When a task resumes, it is already ready for the result, so that the
   * waiting cause no block.
   *
   * @return T
   */
  T await_resume() { return this->task.wait(); }
};
/**
 * @brief A specialization for Task<void>.
 */
template <> class Task<void> {
  friend struct TaskAwaiter<void>;
  friend class EventLoop;
  template <typename U> friend struct TaskPromise;

public:
  using promise_type = TaskPromise<void>;
  using THandle = std::coroutine_handle<promise_type>;
  using Self = Task;

public:
  Task() = default;
  explicit Task(THandle hdl) : co_hdl(hdl) {}
  Task(const Self &) = delete;
  Task(Self &&other) noexcept : co_hdl{std::exchange(other.co_hdl, {})} {}
  ~Task() {
    if (this->co_hdl) {
      this->co_hdl.destroy();
    }
  }
  auto operator=(const Self &) = delete;
  Self &operator=(Self &&other) noexcept {
    Task tmp{};
    tmp.swap(other);
    this->swap(tmp);
    return *this;
  }
  void swap(Self &other) noexcept { std::swap(this->co_hdl, other.co_hdl); }

public:
  /**
   * @brief Block the thread to wait for the result.
   */
  void wait(); // impl see below, due to the circular dependency.

  template <typename F> Task<std::invoke_result_t<F>> then(F f) {
    using U = std::invoke_result_t<F>;
    return [](Task<> t, F ff) -> Task<U> {
      try {
        co_await t;
        co_return ff();
      } catch (...) {
        throw;
      }
    }(std::move(*this), std::move(f));
  }
  template <typename F> Task<> catching(F f) {
    return [](Task<> t, F ff) -> Task<> {
      try {
        co_await t;
      } catch (...) {
        ff(std::current_exception());
      }
      co_return;
    }(std::move(*this), std::move(f));
  }
  template <typename F> Task<> finally(F f) {
    return [](Task<> t, F ff) -> Task<> {
      co_await t;
      ff();
    }(std::move(*this), std::move(f));
  }

private:
  THandle co_hdl;
};

template <typename T> class Task {
  friend struct TaskAwaiter<T>;
  friend class EventLoop;
  template <typename U> friend struct TaskPromise;

public:
  using promise_type = TaskPromise<T>;
  using THandle = std::coroutine_handle<promise_type>;
  using Self = Task;

public:
  Task() = default;
  explicit Task(THandle hdl) : co_hdl(hdl) {}
  Task(const Self &) = delete;
  Task(Self &&other) noexcept : co_hdl{std::exchange(other.co_hdl, {})} {}
  ~Task() {
    if (this->co_hdl) {
      this->co_hdl.destroy();
    }
  }
  auto operator=(const Self &) = delete;
  Self &operator=(Self &&other) noexcept {
    Task tmp{};
    tmp.swap(other);
    this->swap(tmp);
    return *this;
  }
  void swap(Self &other) noexcept { std::swap(this->co_hdl, other.co_hdl); }

public:
  /**
   * @brief Block the thread to wait for the result.
   *
   * @return The result of the task or throw the exception happend within the
   * task.
   */
  T& wait() {
    while (!this->co_hdl.done()) {
      this->co_hdl.resume();
    }
    return this->co_hdl.promise().get();
  }

  template <typename F> Task<std::invoke_result_t<F, T &>> then(F f) {
    using U = std::invoke_result_t<F, T &>;
    return [](Task<T> t, F ff) -> Task<U> {
      try {
        auto res{co_await t};
        co_return ff(res);
      } catch (...) {
        throw;
      }
    }(std::move(*this), std::move(f));
  }
  template <typename F> Task<> catching(F f) {
    return [](Task<T> t, F ff) -> Task<> {
      try {
        co_await t;
      } catch (...) {
        ff(std::current_exception());
      }
      co_return;
    }(std::move(*this), std::move(f));
  }
  template <typename F> Task<> finally(F f) {
    return [](Task<T> t, F ff) -> Task<> {
      co_await t;
      ff();
    }(std::move(*this), std::move(f));
  }

public:
  THandle co_hdl;
};

template <> struct TaskPromise<void> {
  /**
   * @brief A task of void do not need to store the result.
   *
   */
  std::exception_ptr ep;
  std::coroutine_handle<> prev_hdl{};
  /**
   * @brief Tasks are lazy, so they are not ready when they are first awaited.
   *
   * @return std::suspend_always
   */
  std::suspend_always initial_suspend() const noexcept { return {}; }
  /**
   * @brief To reserve the coroutine state, or otherwise the coroutine is
   * destroyed before its result is got.
   *
   * @return std::suspend_always
   */
  FinalAwaiter final_suspend() const noexcept { return {prev_hdl}; }
  /**
   * @brief An awaiter needs no transformation.
   * @return A The very same awaiter.
   */
  template <concepts::Awaiter A> A await_transform(A &&a) const noexcept {
    return std::forward<A>(a);
  }
  /**
   * @brief Transform a task to an awaiter. Once a task is awaited, it is no
   * more needed(because its result becomes the value of the await epression),
   * so it is moved into the awaiter.
   *
   */
  template <typename T> TaskAwaiter<T> await_transform(Task<T> &t) noexcept {
    auto task{std::move(t)};
    task.co_hdl.promise().prev_hdl =
        std::coroutine_handle<TaskPromise<void>>::from_promise(*this);
    return TaskAwaiter<T>{std::move(task)};
  }
  /**
   * @brief The same as above, but for rvalue reference.
   *
   */
  template <typename T> TaskAwaiter<T> await_transform(Task<T> &&t) noexcept {
    auto task{std::move(t)};
    task.co_hdl.promise().prev_hdl =
        std::coroutine_handle<TaskPromise<void>>::from_promise(*this);
    return TaskAwaiter<T>{std::move(task)};
  }
  Task<void> get_return_object() {
    return Task<void>{std::coroutine_handle<TaskPromise>::from_promise(*this)};
  }
  /**
   * @brief If exception happens, store it.
   *
   */
  void unhandled_exception() { this->ep = std::current_exception(); }
  void return_void() {}
  /**
   * @brief If exception happens, throw it.
   *
   */
  void get() {
    if (this->ep) {
      std::rethrow_exception(this->ep);
    }
  }
};

template <typename T> struct TaskPromise {
  /**
   * @brief Either the result nor the exception is stored in the promise. Null
   * exception_ptr represents unfinished coroutine.
   *
   */
  std::variant<std::exception_ptr, T> result;
  std::coroutine_handle<> prev_hdl{};
  /**
   * @brief Tasks are lazy, so they are not ready when they are first awaited.
   *
   * @return std::suspend_always
   */
  std::suspend_always initial_suspend() const noexcept { return {}; }
  /**
   * @brief To reserve the coroutine state, or otherwise the coroutine is
   * destroyed before its result is got.
   *
   * @return std::suspend_always
   */
  FinalAwaiter final_suspend() const noexcept { return {prev_hdl}; }
  /**
   * @brief An awaiter needs no transformation.
   * @return A The very same awaiter.
   */
  template <concepts::Awaiter A> A await_transform(A &&a) const noexcept {
    return std::forward<A>(a);
  }
  /**
   * @brief Transform a task to an awaiter. Once a task is awaited, it is no
   * more needed(because its result becomes the value of the await epression),
   * so it is moved into the awaiter.
   *
   */
  template <typename U> TaskAwaiter<U> await_transform(Task<U> &t) noexcept {
    auto task{std::move(t)};
    task.co_hdl.promise().prev_hdl =
        std::coroutine_handle<TaskPromise<T>>::from_promise(*this);
    return TaskAwaiter<U>{std::move(task)};
  }
  /**
   * @brief The same as above, but for rvalue reference.
   *
   */
  template <typename U> TaskAwaiter<U> await_transform(Task<U> &&t) noexcept {
    auto task{std::move(t)};
    task.co_hdl.promise().prev_hdl =
        std::coroutine_handle<TaskPromise<T>>::from_promise(*this);
    return TaskAwaiter<U>{std::move(task)};
  }
  Task<T> get_return_object() {
    return Task<T>{std::coroutine_handle<TaskPromise>::from_promise(*this)};
  }
  /**
   * @brief If exception happens, store it.
   *
   */
  void unhandled_exception() {
    this->result.template emplace<std::exception_ptr>(std::current_exception());
  }
  /**
   * @brief Store the result of the coroutine.
   *
   */
  template <typename U> void return_value(U &&value) {
    this->result.template emplace<T>(std::forward<U>(value));
  }
  /**
   * @brief Get the result or throw the exception happend in the coroutine.
   * @return T
   */
  T& get() {
    if (this->result.index() == 0 &&
        std::get<std::exception_ptr>(this->result)) {
      std::rethrow_exception(std::get<std::exception_ptr>(this->result));
    }
    return std::get<T>(this->result);
  }
};

inline void Task<void>::wait() {

  while (!this->co_hdl.done()) {
    this->co_hdl.resume();
  }
  this->co_hdl.promise().get();
}
} // namespace cocos
#endif // COCOS_TASK