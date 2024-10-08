#include "../include/generator.hpp"
#include <iostream>
cocos::Generator<int> range_int(int start, int end) {
  for (int num{start}; num < end; ++num) {
    co_yield num;
  }
}

int main() {
  auto g{range_int(0, 10)};
  while (g.move_next()) {
    std::cout << g.current_value() << " ";
  }
  std::cout << "\n";
}