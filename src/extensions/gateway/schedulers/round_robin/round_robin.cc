#include "round_robin.hh"

namespace carrot::gateway::schedulers {

auto RoundRobin::selectNode(const common::Task& task,
                            std::span<common::NodeInfo*> eligible_nodes) -> std::string {
  if (eligible_nodes.empty()) {
    return {};
  }
  auto& cursor = cursors_[task.type];
  if (cursor >= eligible_nodes.size()) {
    cursor = 0;
  }
  auto* chosen = eligible_nodes[cursor];
  cursor = (cursor + 1) % eligible_nodes.size();
  return chosen->id;
}

} // namespace carrot::gateway::schedulers

CARROT_REGISTER_EXTENSION(scheduler, "round_robin", carrot::gateway::schedulers::RoundRobin);
