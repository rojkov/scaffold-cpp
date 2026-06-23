#pragma once

#include <memory>
#include <string>
#include <variant>

#include "carrot/common/types.hh"

namespace carrot::common {

struct AddNode {
  NodeInfo info;
};

struct RemoveNode {
  std::string node_id;
};

struct UpdateNode {
  NodeInfo info;
};

using NodeCommand = std::variant<AddNode, RemoveNode, UpdateNode>;

} // namespace carrot::common
