#pragma ocne
#include <coroutine>
#include <optional>
#include <utility>
#include <ranges>

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
  template <typename Iter>
  static Generator<T> from_iterator(Iter begin, Iter end) {
    for (auto iter{begin}; iter != end; ++iter) {
      co_yield *iter;
    }
  }
  template <typename R> static Generator<T> from_range(R &&range) {
    auto r{std::forward<R>(range)};
    return from_iterator(std::ranges::begin(r), std::ranges::end(r));
  }

public:
  /**
   * @brief returns the next element of the genrator, if there's no more
   * element, nullopt will be returned.
   *
   * @return std::optional<T>
   */
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
  /**
   * @brief Mapping each element into another value using the function f.
   *
   * @tparam F the type of mapping function.
   * @param f the mapping function
   * @return Generator<std::invoke_result_t<F, T &>> the generator generats
   * mapped elements, into which the original generator is moved into.
   */
  template <typename F> Generator<std::invoke_result_t<F, T &>> map(F f) {
    return [](Generator<T> g, F f) -> Generator<std::invoke_result_t<F, T &>> {
      while (auto opt{g.next()}) {
        co_yield f(*opt);
      }
    }(std::move(*this), std::move(f));
  }
  /**
   * @brief filter the elements uisng `f` as the predicate. only if elements
   * where `f(elem)` is `true` will be generated.
   *
   * @tparam F the type of predicate function
   * @param f predicate function.
   * @return Generator<T> the new generator, into which `*this` is moved into.
   */
  template <typename F> Generator<T> filter(F f) {
    return [](Generator<T> g, F f) -> Generator<T> {
      while (auto opt{g.next()}) {
        if (f(*opt)) {
          co_yield *opt;
        }
      }
    }(std::move(*this), std::move(f));
  }
  /**
   * @brief traverse the elements from the generator. the original generator
   * will be consumed.
   *
   * @tparam F
   * @param f
   */
  template <typename F> void for_each(F f) {
    while (auto opt{this->next()}) {
      f(*opt);
    }
  }
  /**
   * @brief fold the element sequence into one value.
   *
   * @tparam R return type
   * @tparam F the type of aggregate function.
   * @param initial_val
   * @param f the aggregate function, whose first argument is the current value,
   * and the second is the next value.
   * @return R the final value.
   */
  template <typename R, typename F> R fold(R initial_val, F f) {
    R ret{std::move(initial_val)};
    while (auto opt{this->next()}) {
      ret = f(ret, *opt);
    }
    return ret;
  }
  /**
   * @brief take the first n elements of the generator. the rest is not to be
   * evaluated.
   *
   * @param n the count you need.
   * @return Generator<T> the generator that generates the first n elements,
   * into which the original generator is moved.
   */
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
  /**
   * @brief similar to take(), but using the given predicate, until `f(elem)` is
   * false.
   *
   * @tparam F the type of the predicate.
   * @param f the predicate.
   * @return Generator<T> the generator of elements, into which the original
   * generator is moved.
   */
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
  /**
   * @brief reduce the element sequence into one value. similar to fold, but
   * using the first value of the generator as the initial value.
   *
   * @tparam F the type of the aggregate function.
   * @param f the aggregate function, whose first argument is the current value
   * and the second is the next value of the generator.
   * @return std::optional<T> nullopt if the generator is empty.
   */
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
  /**
   * @brief silimar to fold, but generates the each intermediate result rather
   * than returns the final value.
   *
   * @tparam R the result type
   * @tparam F the aggregate function.
   * @param initial the initial value.
   * @param f the aggregate function.
   * @return Generator<R> the generator that genetates each intermediate result
   * while folding.
   */
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