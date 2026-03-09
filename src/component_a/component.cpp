#include "src/component_a/component.hh"

#include <iostream>
#include <string>

namespace carrot {

Agent::Agent() { test_str_ = const_cast<char*>(std::string("Test string").c_str()); }

int Agent::Run() {
  std::cout << test_str_ << std::endl;

  if (true)
    std::cout << "s";
  return 1;
}

} // namespace carrot
