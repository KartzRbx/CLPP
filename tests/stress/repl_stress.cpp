#include "clpp/repl.hpp"

#include <iostream>
#include <sstream>
#include <string>

int main() {
  int failures = 0;
  for (int i = 0; i < 600; ++i) {
    std::istringstream in("post(" + std::to_string(i) + ");\nexit\n");
    std::ostringstream out;
    std::ostringstream err;
    if (clpp::run_repl(in, out, err) != 0 || out.str() != std::to_string(i) + "\n") {
      ++failures;
    }
  }
  for (int i = 0; i < 200; ++i) {
    std::istringstream in("link @clpp.math as Math; post(Math.abs(0 - " + std::to_string(i) + "));\nexit\n");
    std::ostringstream out;
    std::ostringstream err;
    if (clpp::run_repl(in, out, err) != 0 || out.str() != std::to_string(i) + "\n") {
      ++failures;
    }
  }
  for (int i = 0; i < 200; ++i) {
    std::istringstream in("int x = \"v" + std::to_string(i) + "\";\nexit\n");
    std::ostringstream out;
    std::ostringstream err;
    if (clpp::run_repl(in, out, err) != 0 || !out.str().empty()) {
      ++failures;
    }
  }
  std::cout << "ran 1000 failures " << failures << "\n";
  return failures == 0 ? 0 : 1;
}
