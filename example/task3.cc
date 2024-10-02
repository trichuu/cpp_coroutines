#include "../include/sleep.hpp"
#include "../include/task.hpp"
#include <format>
#include <iostream>


#define print(...) (std::cout << std::format(__VA_ARGS__))
using namespace std::chrono_literals;

cocos::Task<> pstr(int num) {
  print("Hello from task{}.\n", num);
  co_return;
}

cocos::Task<> throws() {
  co_await cocos::sleep(1s);
  co_await pstr(1);
  throw std::runtime_error("This is an exception.");
}

cocos::Task<> just() {
  co_await pstr(2);
  co_await cocos::sleep(2s);
  co_return;
}

int main() {
  auto &loop = cocos::EventLoop::get_loop();
  auto t1 = throws()
                .then([]() { print("Ok t1\n"); })
                .catching([](auto &&) { print("Except from t1\n"); })
                .finally([]() { print("Finally from t1\n"); });
  auto t2 = just()
                .then([]() { print("Ok t2\n"); })
                .catching([](auto &&) { print("Except from t2\n"); })
                .finally([]() { print("Finally from t2\n"); });
  loop.add_task(t1);
  loop.add_task(t2);
  loop.run();
}