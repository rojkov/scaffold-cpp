#include "static_discovery.hh"

#include <cstdint>

#include <yaml-cpp/yaml.h>

namespace carrot::gateway::discovery {

Static::Static(common::FactoryContext&, const common::Config& cfg) {
  const auto* nodes = std::any_cast<YAML::Node>(&cfg);
  if (nodes == nullptr) {
    return;
  }

  for (const auto& node : (*nodes)["nodes"]) {
    common::NodeInfo info;
    info.id = node["id"].as<std::string>();
    info.address = node["address"].as<std::string>();
    if (node["task_types"]) {
      for (const auto& t : node["task_types"]) {
        info.task_types.push_back(t.as<std::string>());
      }
    }
    if (node["max_capacity"]) {
      for (const auto& c : node["max_capacity"]) {
        info.max_capacity.push_back({c["type"].as<std::string>(), c["amount"].as<uint64_t>()});
      }
    }
    if (handler_) {
      handler_->onNodeAdded(info);
    }
  }
}

} // namespace carrot::gateway::discovery

CARROT_REGISTER_EXTENSION(node_discovery, "static", carrot::gateway::discovery::Static);
