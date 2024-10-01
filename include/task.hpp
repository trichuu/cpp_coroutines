#pragma once
#include "coroutine_concepts.hpp"
#include "eventloop.hpp"
#include <coroutine>
#include <exception>
#include <type_traits>
#include <utility>
#include <variant>

namespace cocos {
template <typename T = void> struct TaskPromise;
template <typename T = void> class Task;
template <typename T> struct TaskAwaiter;

template <typename T> struct TaskAwaiter {
  Task<T> task;

  bool await_ready() const noexcept { return false; }
  std::coroutine_handle<>
  await_suspend(std::coroutine_handle<> hdl) const noexcept {
    EventLoop::get_loop().add_task(hdl);
    return std::noop_coroutine();
  }
  T await_resume() { return this->task.wait(); }
};

template <> class Task<void> {
  friend struct TaskAwaiter<void>;

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
  void wait(); // impl see below
  std::coroutine_handle<> get_handle() const noexcept { return this->co_hdl; }
  
  template <typename F> Task<std::invoke_result_t<F>> then(F f) {
    using U = std::invoke_result_t<F>;
    return [](Task<> t, F f) -> Task<U> {
      try {
        co_await t;
        co_return f();
      } catch (...) {
        throw;
      }
    }(std::move(*this), std::move(f));
  }
  template <typename F> Task<> catching(F f) {
    return [](Task<> t, F f) -> Task<> {
      try {
        co_await t;
      } catch (...) {
        f(std::current_exception());
      }
      co_return;
    }(std::move(*this), std::move(f));
  }
  template <typename F> Task<> finally(F f) {
    return [](Task<> t, F f) -> Task<> {
      co_await t;
      f();
    }(*this, std::move(f));
  }

private:
  THandle co_hdl;
};

template <typename T> class Task {
  friend struct TaskAwaiter<T>;

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
  T wait() {
    while (!this->co_hdl.done()) {
      this->co_hdl.resume();
    }
    return std::move(this->co_hdl.promise().get());
  }
  std::coroutine_handle<> get_handle() const noexcept { return this->co_hdl; }

  template <typename F> Task<std::invoke_result_t<F, T &>> then(F f) {
    using U = std::invoke_result_t<F, T &>;
    return [](Task<T> t, F f) -> Task<U> {
      try {
        auto res{co_await t};
        co_return f(res);
      } catch (...) {
        throw;
      }
    }(std::move(*this), std::move(f));
  }
  template <typename F> Task<> catching(F f) {
    return [](Task<T> t, F f) -> Task<> {
      try {
        co_await t;
      } catch (...) {
        f(std::current_exception());
      }
      co_return;
    }(std::move(*this), std::move(f));
  }
  template <typename F> Task<> finally(F f) {
    return [](Task<T> t, F f) -> Task<> {
      co_await t;
      f();
    }(*this, std::move(f));
  }

private:
  THandle co_hdl;
};

struct TaskPrimiseBase {
  std::suspend_always initial_suspend() const noexcept { return {}; }
  std::suspend_always final_suspend() const noexcept { return {}; }
  template <concepts::Awaiter A> A await_transform(A &&a) const noexcept {
    return std::forward<A>(a);
  }
  template <typename T>
  TaskAwaiter<T> await_transform(Task<T> &t) const noexcept {
    return TaskAwaiter<T>{std::move(t)};
  }
  template <typename T>
  TaskAwaiter<T> await_transform(Task<T> &&t) const noexcept {
    return TaskAwaiter<T>{std::move(t)};
  }
};

template <> struct TaskPromise<void> : public TaskPrimiseBase {
  std::exception_ptr ep;

  Task<void> get_return_object() {
    return Task<void>{std::coroutine_handle<TaskPromise>::from_promise(*this)};
  }
  void unhandled_exception() { this->ep = std::current_exception(); }
  void return_void() {}
  void get() {
    if (this->ep) {
      std::rethrow_exception(this->ep);
    }
  }
};

template <typename T> struct TaskPromise : public TaskPrimiseBase {
  std::variant<std::exception_ptr, T> result;

  Task<T> get_return_object() {
    return Task<T>{std::coroutine_handle<TaskPromise>::from_promise(*this)};
  }
  void unhandled_exception() {
    this->result.template emplace<std::exception_ptr>(std::current_exception());
  }
  template <typename U> void return_value(U &&value) {
    this->result.template emplace<T>(std::forward<U>(value));
  }
  T get() {
    if (this->result.index() == 0 &&
        std::get<std::exception_ptr>(this->result)) {
      std::rethrow_exception(std::get<std::exception_ptr>(this->result));
    }
    return std::move(std::get<T>(this->result));
  }
};

inline void Task<void>::wait() {

  while (!this->co_hdl.done()) {
    this->co_hdl.resume();
  }
  this->co_hdl.promise().get();
}
} // namespace cocos