#pragma once
#include <concepts>
#include <coroutine>
namespace cocos::concepts {
template <typename A>
concept Awaiter = requires(A a, std::coroutine_handle<> h) {
  { a.await_ready() } -> std::convertible_to<bool>;
  a.await_suspend(h);
  a.await_resume();
};
template <typename A>
concept Awaitable = Awaiter<A> || requires(A a) {
  { a.operator co_await() } -> Awaiter;
} || requires(A a) {
  { operator co_await(static_cast<A&&>(a)) } -> Awaiter;
};
} // namespace cocos::concepts