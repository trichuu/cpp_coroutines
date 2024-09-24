#pragma ocne
#include <coroutine>
#include <optional>
#include <utility>

namespace cocos {
template <typename T> class FunctionalGenerator;

template <typename T> class Generator {
public:
  struct promise_type {
    union {
      T value{};
    };

    promise_type() noexcept = default;
    ~promise_type() noexcept = default;
    promise_type(promise_type&&) noexcept = default;

    std::suspend_always initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    Generator<T> get_return_object() {
      return Generator<T>(
          std::coroutine_handle<promise_type>::from_promise(*this));
    }
    void unhandled_exception() { throw; }
    template <typename U> std::suspend_always yield_value(U &&val) {
      std::construct_at(&(this->value), std::forward<U>(val));
      return {};
    }
    void return_void() {}
  };

private:
  std::coroutine_handle<promise_type> prom_handle;

private:
  Generator() = default;
  explicit Generator(std::coroutine_handle<promise_type> handle)
      : prom_handle(handle) {}

public:
  Generator(const Generator &) = delete;
  Generator(Generator &&other) noexcept
      : prom_handle(std::exchange(other.prom_handle,
                                  std::coroutine_handle<promise_type>{})) {}
  auto operator=(const Generator &) = delete;
  Generator &operator=(Generator &&other) noexcept {
    std::destroy_at(this);
    std::construct_at(this, std::move(other));
  }

public:
  ~Generator() {
    if (this->prom_handle) {
      this->prom_handle.destroy();
    }
  }

public:
  std::optional<T> next() {
    if (this->prom_handle.done()) {
      return std::nullopt;
    }
    this->prom_handle.resume();
    if (this->prom_handle.done()) {
      return std::nullopt;
    }
    T val = std::move(this->prom_handle.promise().value);
    std::destroy_at(&(this->prom_handle.promise().value));
    return std::make_optional(std::move(val));
  }
  template <typename F>
  FunctionalGenerator<std::invoke_result_t<F, T &>> map(F f) {
    auto prom{std::move(*this)};
    co_await std::suspend_always{};
    while (auto opt{prom.next()}) {
      co_yield f(*opt);
    }
  }
  template <typename F> FunctionalGenerator<T> filter(F f) {
    auto prom{std::move(*this)};
    co_await std::suspend_always{};
    while (auto opt{prom.next()}) {
      if (f(*opt)) {
        co_yield *opt;
      }
    }
  }
  template <typename F> void for_each(F f) {
    while (auto opt{this->next()}) {
      f(*opt);
    }
  }
  template <typename R, typename F> R fold(R initial_val, F f) {
    R ret{std::move(initial_val)};
    while (auto opt{this->next()}) {
      ret = f(ret, *opt);
    }
    return ret;
  }
  FunctionalGenerator<T> take(std::size_t n) {
    auto prom{std::move(*this)};
    co_await std::suspend_always{};
    while (auto opt{prom.next()}) {
      if (n == 0) {
        break;
      }
      n -= 1;
      co_yield *opt;
    }
  }
  template <typename F> FunctionalGenerator<T> take_while(F f) {
    auto prom{std::move(*this)};
    co_await std::suspend_always{};
    while (auto opt{prom.next()}) {
      if (f(*opt) == true) {
        co_yield *opt;
      } else {
        break;
      }
    }
  }
};

template <typename T>
class FunctionalGenerator { // bisically the same as Generator, except for
                            // initial_suspend
public:
  struct promise_type {
    union {
      T value{};
    };

    std::suspend_never initial_suspend() noexcept {
      return {};
    } // to make sure original generator move immediately after
      // FunctionalGenerator constructed.
    std::suspend_always final_suspend() noexcept { return {}; }
    FunctionalGenerator<T> get_return_object() {
      return FunctionalGenerator<T>(
          std::coroutine_handle<promise_type>::from_promise(*this));
    }
    void unhandled_exception() { throw; }
    template <typename U> std::suspend_always yield_value(U &&val) {
      std::construct_at(&(this->value), std::forward<U>(val));
      return {};
    }
    void return_void() {}
  };

private:
  std::coroutine_handle<promise_type> prom_handle;

private:
  FunctionalGenerator() = default;
  explicit FunctionalGenerator(std::coroutine_handle<promise_type> handle)
      : prom_handle(handle) {}

public:
  FunctionalGenerator(const FunctionalGenerator &) = delete;
  FunctionalGenerator(FunctionalGenerator &&other) noexcept
      : prom_handle(std::exchange(other.prom_handle,
                                  std::coroutine_handle<promise_type>{})) {}
  auto operator=(const FunctionalGenerator &) = delete;
  FunctionalGenerator &operator=(FunctionalGenerator &&other) noexcept {
    std::destroy_at(this);
    std::construct_at(this, std::move(other));
  }

public:
  ~FunctionalGenerator() {
    if (this->prom_handle) {
      this->prom_handle.destroy();
    }
  }

public:
  std::optional<T> next() {
    if (this->prom_handle.done()) {
      return std::nullopt;
    }
    this->prom_handle.resume();
    if (this->prom_handle.done()) {
      return std::nullopt;
    }
    T val = std::move(this->prom_handle.promise().value);
    std::destroy_at(&(this->prom_handle.promise().value));
    return std::make_optional(std::move(val));
  }
  template <typename F>
  FunctionalGenerator<std::invoke_result_t<F, T &>> map(F f) {
    auto prom{std::move(*this)};
    co_await std::suspend_always{};
    while (auto opt{prom.next()}) {
      co_yield f(*opt);
    }
  }
  template <typename F> FunctionalGenerator<T> filter(F f) {
    auto prom{std::move(*this)};
    co_await std::suspend_always{};
    while (auto opt{prom.next()}) {
      if (f(*opt)) {
        co_yield *opt;
      }
    }
  }
  template <typename F> void for_each(F f) {
    while (auto opt{this->next()}) {
      f(*opt);
    }
  }
  template <typename R, typename F> R fold(R initial_val, F f) {
    R ret{std::move(initial_val)};
    while (auto opt{this->next()}) {
      ret = f(ret, *opt);
    }
    return ret;
  }
  FunctionalGenerator<T> take(std::size_t n) {
    auto prom{std::move(*this)};
    co_await std::suspend_always{};
    while (auto opt{prom.next()}) {
      if (n == 0) {
        break;
      }
      n -= 1;
      co_yield *opt;
    }
  }
  template <typename F> FunctionalGenerator<T> take_while(F f) {
    auto prom{std::move(*this)};
    co_await std::suspend_always{};
    while (auto opt{prom.next()}) {
      if (f(*opt) == true) {
        co_yield *opt;
      } else {
        break;
      }
    }
  }
};
} // namespace cocos