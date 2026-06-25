#pragma once

#include <vector>

#include "carrot/common/extension_registry.hh"
#include "carrot/gateway/node_discovery.hh"

namespace carrot::gateway::discovery {

class Static : public NodeDiscovery {
public:
  explicit Static(common::FactoryContext& ctx, const common::Config& cfg);

  void setHandler(Handler& handler) override;

private:
  Handler* handler_{nullptr};
  std::vector<common::NodeInfo> nodes_;
};

} // namespace carrot::gateway::discovery
