#pragma once

#include <string>
#include <unordered_map>

#include "carrot/common/extension_registry.hh"
#include "carrot/gateway/scheduler.hh"

namespace carrot::gateway::schedulers {

class RoundRobin : public Scheduler {
public:
  explicit RoundRobin(common::FactoryContext&, const common::Config&) {}

  auto selectNode(const common::Task& task,
                  std::span<common::NodeInfo*> eligible_nodes) -> std::string override;

private:
  std::unordered_map<std::string, size_t> cursors_;
};

} // namespace carrot::gateway::schedulers
