#pragma once
#include <coroutine>
#include <exception>
#include <ranges>
#include <utility>
#include <variant>
#include <optional>
namespace cocos {
template <typename T> class Generator;

template <typename T> struct GeneratorPromise {
  std::variant<std::monostate, T, std::exception_ptr> result;

  constexpr std::suspend_always initial_suspend() const noexcept { return {}; }
  constexpr std::suspend_always final_suspend() const noexcept { return {}; }
  Generator<T> get_return_object() {
    return Generator{
        std::coroutine_handle<GeneratorPromise>::from_promise(*this)};
  }
  void unhandled_exception() {
    this->result.template emplace<std::exception_ptr>(std::current_exception());
  }
  template <typename U> std::suspend_always yield_value(U &&val) {
    this->result.template emplace<T>(std::forward<U>(val));
    return {};
  }
  T &get_or_throw() {
    auto p{std::get_if<T>(&(this->result))};
    if (p) {
      return *p;
    } else {
      std::rethrow_exception(std::get<2>(this->result));
    }
  }
};
/**
 * @brief A lazily evaluating generator.
 *
 * @tparam T The type to be generated.
 */
template <typename T> class Generator {
public:
  using promise_type = GeneratorPromise<T>;
  using Self = Generator;

private:
  using THandle = std::coroutine_handle<promise_type>;

public:
  /**
   * @brief construct a Generator with no underlying coroutine.
   *
   */
  constexpr Generator() : co_handle{} {}
  /**
   * @brief Construct a new Generator object, with the underlying coroutine
   * represented by `handle`.
   *
   * @param handle The underlying coroutine of the generator.
   */
  explicit Generator(THandle handle) : co_handle{handle} {}
  /**
   * @brief Each generator has an exclusive ownership of the undelying
   * coroutine. So that copy construction is deleted.
   *
   */
  Generator(const Self &) = delete;
  Generator(Self &&other) : co_handle{std::exchange(other.co_handle, {})} {}
  /**
   * @brief The same reason as deleted copy constructor.
   *
   * @return auto
   */
  auto operator=(const Self &) = delete;
  Self &operator=(Self &&other) {
    Self tmp{};
    tmp.swap(other);
    this->swap(other);
    return *this;
  }
  /**
   * @brief Destroy the underlying coroutine.
   *
   */
  ~Generator() {
    if (this->co_handle) {
      this->co_handle.destroy();
    }
  }
  void swap(Self &other) { std::swap(this->co_handle, other.co_handle); }
  bool has_coroutine() const noexcept { return this->co_handle; }

public:
  /**
   * @brief Move comuse generator to yield next value.
   *
   * @return true if there is a next value.
   * @return false if there is not a naxt value.
   */
  bool move_next() {
    this->co_handle.resume();
    return !(this->co_handle.done());
  }
  T &current_value() { return this->co_handle.promise().get_or_throw(); }
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
      while (g.move_next()) {
        co_yield f(g.current_value());
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
      while (g.move_next()) {
        if (f(g.current_value())) {
          co_yield g.current_value();
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
    while (this->move_next()) {
      f(this->current_value());
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
    while (this->move_next()) {
      ret = f(ret, this->current_value());
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
      while (prom.move_next()) {
        if (n == 0) {
          break;
        }
        n -= 1;
        co_yield prom.current_value();
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
      while (prom.move_next()) {
        if (f(prom.current_value())) {
          co_yield prom.current_value();
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
   * `NOTE`: each value is moved into `f`.
   * @return std::optional<T> nullopt if the generator is empty.
   */
  template <typename F> std::optional<T> reduce(F f) {
    if (!this->move_next()) {
      return std::nullopt;
    }
    auto val{std::move(this->current_value())};
    while (this->move_next()) {
      val = f(val, std::move(this->current_value()));
    }
    return std::make_optional(val);
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
      while (prom.move_next()) {
        val = f(val, prom.current_value());
        co_yield val;
      }
    }(std::move(*this), std::move(initial), std::move(f));
  }

private:
  std::coroutine_handle<promise_type> co_handle;
};
} // namespace cocos