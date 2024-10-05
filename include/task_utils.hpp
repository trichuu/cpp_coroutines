#ifndef COCOS_TASK_UTILS
#define COCOS_TASK_UTILS
#include "eventloop.hpp"
#include "task.hpp"
#include <coroutine>
#include <iterator>
#include <vector>

namespace cocos {
namespace detail {
template <typename T> struct TaskRet;
template <typename T> struct TaskRet<Task<T>> {
  using type = T;
};
} // namespace detail

template <typename T> Task<T> just(T &&value) {
  co_return std::forward<T>(value);
}
} // namespace cocos
#endif