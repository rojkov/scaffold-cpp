#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace carrot::common {

using Chunk = std::vector<std::byte>;

struct Requirement {
  std::string type;
  uint64_t amount;
};

struct Task {
  std::string type;
  std::vector<std::byte> body;
  std::optional<std::string> function;
  std::optional<std::string> result_endpoint;
  std::vector<Requirement> requirements;
};

struct NodeInfo {
  std::string id;
  std::string address;
  std::vector<std::string> task_types;
  std::vector<Requirement> max_capacity;
};

} // namespace carrot::common
