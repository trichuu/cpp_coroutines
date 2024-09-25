#pragma once
#include <concepts>
namespace cocos::concepts {
template <typename A>
concept Awaiter = requires(A a) {
  { a.await_ready() } -> std::same_as<bool>;
  a.await_suspend();
  a.await_resume();
};
template <typename A>
concept Awaitable = Awaiter<A> || requires(A a) {
  { a.operator co_await } -> Awaiter;
};
} // namespace cocos::concepts