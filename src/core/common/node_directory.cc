#include "carrot/common/node_directory.hh"

#include <algorithm>

namespace carrot::common {

void NodeDirectory::addNode(NodeInfo info) {
  auto id = info.id;
  auto ptr = std::make_unique<NodeInfo>(std::move(info));
  by_id_[id] = ptr.get();
  for (const auto& type : ptr->task_types) {
    by_type_[type].push_back(ptr.get());
  }
  nodes_.push_back(std::move(ptr));
}

void NodeDirectory::removeNode(std::string_view id) {
  auto key = std::string(id);
  auto it = by_id_.find(key);
  if (it == by_id_.end()) {
    return;
  }
  auto* node = it->second;

  for (const auto& type : node->task_types) {
    auto& vec = by_type_[type];
    std::erase(vec, node);
    if (vec.empty()) {
      by_type_.erase(std::string(type));
    }
  }

  by_id_.erase(it);
  auto node_it = std::find_if(nodes_.begin(), nodes_.end(),
                              [node](const auto& p) { return p.get() == node; });
  if (node_it != nodes_.end()) {
    nodes_.erase(node_it);
  }
}

void NodeDirectory::updateNode(const NodeInfo& info) {
  auto* existing = byId(info.id);
  if (existing == nullptr) {
    addNode(info);
    return;
  }

  for (const auto& type : existing->task_types) {
    auto& vec = by_type_[type];
    std::erase(vec, existing);
  }

  *existing = info;

  for (const auto& type : existing->task_types) {
    by_type_[type].push_back(existing);
  }
}

} // namespace carrot::common
