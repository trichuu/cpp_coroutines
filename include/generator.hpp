#pragma ocne
#include <coroutine>
#include <optional>
#include <utility>

namespace cocos {
template <typename T> class Generator {
public:
  struct promise_type {
    union {
      T value{};
    };

    promise_type() noexcept = default;
    ~promise_type() noexcept = default;
    promise_type(promise_type &&) noexcept = delete;

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
  template <typename F> Generator<std::invoke_result_t<F, T &>> map(F f) {
    return [](Generator<T> g, F f) -> Generator<std::invoke_result_t<F, T &>> {
      while (auto opt{g.next()}) {
        co_yield f(*opt);
      }
    }(std::move(*this), std::move(f));
  }
  template <typename F> Generator<T> filter(F f) {
    return [](Generator<T> g, F f) -> Generator<T> {
      while (auto opt{g.next()}) {
        if (f(*opt)) {
          co_yield *opt;
        }
      }
    }(std::move(*this), std::move(f));
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
  Generator<T> take(std::size_t n) {
    return [](Generator<T> prom, std::size_t n) -> Generator<T> {
      while (auto opt{prom.next()}) {
        if (n == 0) {
          break;
        }
        n -= 1;
        co_yield *opt;
      }
    }(std::move(*this), n);
  }
  template <typename F> Generator<T> take_while(F f) {
    return [](Generator<T> prom, F f) -> Generator<T> {
      while (auto opt{prom.next()}) {
        if (f(*opt)) {
          co_yield *opt;
        } else {
          break;
        }
      }
    }(std::move(*this), std::move(f));
  }
  template <typename F> std::optional<T> reduce(F f) {
    auto val{this->next()};
    if (!val) {
      return std::nullopt;
    }
    auto ret{*val};
    while (auto opt{this->next()}) {
      ret = f(ret, *opt);
    }
    return std::make_optional(ret);
  }
  template <typename R, typename F> Generator<R> scan(R initial, F f) {
    return [](Generator<T> prom, R initial, F f) -> Generator<R> {
      R val{std::move(initial)};
      while (auto opt{prom.next()}) {
        val = f(val, *opt);
        co_yield val;
      }
    }(std::move(*this), std::move(initial), std::move(f));
  }
};
} // namespace cocos