#pragma once

namespace carrot {

class Agent final {
public:
  Agent();
  int Run();

private:
  char* test_str_{nullptr};
};

} // namespace carrot
