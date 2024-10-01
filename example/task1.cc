#include "../include/sleep.hpp"
#include "../include/task.hpp"
#include <format>
#include <iostream>


#define print(...) (std::cout << std::format(__VA_ARGS__))

using namespace std::chrono_literals;

cocos::Task<double> task1() {
  print("Task 1 started\n");
  co_await cocos::sleep(1s);
  print("Task 1 finished\n");
  co_return 2.5;
}
cocos::Task<int> task2() {
  print("Task 2 started\n");
  co_await cocos::sleep(2s);
  print("Task 2 finished\n");
  co_return 42;
}

int main() {
  auto &loop = cocos::EventLoop::get_loop();
  auto t1 = task1();
  auto t2 = task2();
  auto now = std::chrono::steady_clock::now();
  loop.add_task(t1.get_handle());
  loop.add_task(t2.get_handle());
  loop.run();
  auto dur = std::chrono::steady_clock::now() - now;
  print("Total time: {}\n", dur);
  print("{}, {}\n", t1.wait(), t2.wait());
  return 0;
}