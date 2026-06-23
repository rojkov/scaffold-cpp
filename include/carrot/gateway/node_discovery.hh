#pragma once

#include <functional>
#include <string>
#include <string_view>

#include "carrot/common/types.hh"

namespace carrot::gateway {

class NodeDiscovery {
public:
  struct Handler {
    virtual void onNodeAdded(const common::NodeInfo& info) = 0;
    virtual void onNodeRemoved(std::string_view node_id) = 0;
    virtual void onNodeUpdated(const common::NodeInfo& info) = 0;
    virtual ~Handler() = default;
  };

  NodeDiscovery() = default;
  virtual ~NodeDiscovery() = default;

  NodeDiscovery(const NodeDiscovery&) = delete;
  auto operator=(const NodeDiscovery&) -> NodeDiscovery& = delete;
  NodeDiscovery(NodeDiscovery&&) noexcept = delete;
  auto operator=(NodeDiscovery&&) noexcept -> NodeDiscovery& = delete;

  virtual void setHandler(Handler& handler) = 0;

  using interface_type = NodeDiscovery;
};

} // namespace carrot::gateway
