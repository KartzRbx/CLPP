#include "clpp/compiler.hpp"
#include "clpp/vm.hpp"

#include <iostream>
#include <sstream>
#include <string>

int main() {
  int failures = 0;
  for (int i = 0; i < 800; ++i) {
    const std::string source = "def Wallet { number Coins=" + std::to_string(i) +
        "; } void Wallet::Add() { @this.Coins=@this.Coins+1; } "
        "number Wallet::Read() { return @this.Coins; } "
        "auto wallet=Wallet {}; wallet.Add(); post(wallet.Read());";
    const clpp::CompileResult compiled = clpp::Compiler{}.compile(source);
    std::ostringstream out;
    clpp::VirtualMachine vm;
    vm.set_output(out);
    if (!compiled.ok()) {
      ++failures;
      continue;
    }
    vm.load(compiled.chunk);
    if (!vm.run() || out.str() != std::to_string(i + 1) + "\n") {
      ++failures;
    }
  }
  for (int i = 0; i < 200; ++i) {
    const std::string source = "def Wallet { number Coins=0; } number Wallet::Read() { return @this.M" + std::to_string(i) + "; }";
    const clpp::CompileResult compiled = clpp::Compiler{}.compile(source);
    // Receiver fields are checked even when no object invokes the method.
    if (compiled.ok()) {
      ++failures;
    }
  }
  std::cout << "ran 1000 failures " << failures << "\n";
  return failures == 0 ? 0 : 1;
}
