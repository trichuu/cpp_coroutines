
#include <format>
#include <iostream>
#include <print>

#define print(...) (std::cout << std::format(__VA_ARGS__))
#include "../include/sleep.hpp"
#include "../include/task.hpp"

#include <thread>

using namespace std::chrono_literals;

cocos::Task<> task0() {
  print("Hello from task0.\n");
  co_return;
}

cocos::Task<double> task1() {
  auto start = std::chrono::steady_clock::now();
  print("Task 1 started at thread {}.\n", std::this_thread::get_id());
  co_await cocos::sleep(2s);
  print("Task 1 finished at thread {}, slept for {}.\n",
        std::this_thread::get_id(), std::chrono::steady_clock::now() - start);
  co_return 2.5;
}
cocos::Task<int> task2() {
  auto start = std::chrono::steady_clock::now();
  print("Task 2 started at thread {}.\n", std::this_thread::get_id());
  co_await cocos::sleep(1s);
  co_await task0();
  print("Task 2 finished at thread {}, slept for {}.\n",
        std::this_thread::get_id(), std::chrono::steady_clock::now() - start);
  co_return 42;
}

int main() {
  auto &loop = cocos::EventLoop::get_loop();
  auto t1 = task1();
  auto t2 = task2();
  print("Program begins at Main thread: {}.\n", std::this_thread::get_id());
  auto now = std::chrono::steady_clock::now();
  loop.add_task(t1);
  loop.add_task(t2);
  loop.run();
  auto dur = std::chrono::steady_clock::now() - now;
  print("Total time: {}\n", dur);
  print("{}, {}\n", t1.wait(), t2.wait());
  return 0;
}