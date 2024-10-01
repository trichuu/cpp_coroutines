#pragma once
#include <concepts>
#include <coroutine>
namespace cocos::concepts {

namespace detail {
template <typename T, typename... Types>
concept OneOf = (std::same_as<T, Types> || ...);
template <typename T> struct is_coroutine_handle : std::false_type {};
template <typename T>
struct is_coroutine_handle<std::coroutine_handle<T>> : std::true_type {};
template <typename T>
concept AwaitSuspendSatisfied =
    OneOf<T, void, bool> || is_coroutine_handle<T>::value;
} // namespace detail

template <typename A>
concept Awaiter = requires(A a, std::coroutine_handle<> h) {
  { a.await_ready() } -> std::convertible_to<bool>;
  { a.await_suspend(h) } -> detail::AwaitSuspendSatisfied;
  a.await_resume();
};
template <typename A>
concept Awaitable = Awaiter<A> || requires(A a) {
  { a.operator co_await() } -> Awaiter;
} || requires(A a) {
  { operator co_await(static_cast<A &&>(a)) } -> Awaiter;
};
} // namespace cocos::concepts