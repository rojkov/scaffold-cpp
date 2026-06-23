#pragma once

#include "carrot/common/extension_registry.hh"
#include "carrot/gateway/node_discovery.hh"

namespace carrot::gateway::discovery {

class Static : public NodeDiscovery {
public:
  explicit Static(common::FactoryContext& ctx, const common::Config& cfg);

  void setHandler(Handler& handler) override { handler_ = &handler; }

private:
  Handler* handler_{nullptr};
};

} // namespace carrot::gateway::discovery
