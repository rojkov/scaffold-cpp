#pragma once

#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "carrot/common/types.hh"

namespace carrot::common {

class NodeDirectory {
public:
  void addNode(NodeInfo info);
  void removeNode(std::string_view id);
  void updateNode(const NodeInfo& info);

  auto byId(std::string_view id) -> NodeInfo* {
    auto key = std::string(id);
    auto it = by_id_.find(key);
    return it != by_id_.end() ? it->second : nullptr;
  }

  auto byType(std::string_view type) -> std::span<NodeInfo*> {
    return by_type_[std::string(type)];
  }

private:
  std::vector<std::unique_ptr<NodeInfo>> nodes_;
  std::unordered_map<std::string, NodeInfo*> by_id_;
  std::unordered_map<std::string, std::vector<NodeInfo*>> by_type_;
};

} // namespace carrot::common
