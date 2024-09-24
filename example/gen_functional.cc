#include "../include/generator.hpp"
#include <iostream>

cocos::Generator<int> range_int(int start, int end) {
  for (int num{start}; num < end; ++num) {
    co_yield num;
  }
}

int main() {
  auto g = range_int(0, 10)
               .filter([](int i) { return i % 2 == 0; })
               .map([](int i) { return i * i; })
               .take(3);
  while (auto opt{g.next()}) {
    std::cout << *opt << std::endl;
  }
  std::cout << "\n";
}
