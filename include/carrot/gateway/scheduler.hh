#pragma once

#include <span>
#include <string>

#include "carrot/common/types.hh"

namespace carrot::gateway {

class Scheduler {
public:
  Scheduler() = default;
  virtual ~Scheduler() = default;

  Scheduler(const Scheduler&) = delete;
  auto operator=(const Scheduler&) -> Scheduler& = delete;
  Scheduler(Scheduler&&) noexcept = delete;
  auto operator=(Scheduler&&) noexcept -> Scheduler& = delete;

  virtual auto selectNode(const common::Task& task,
                          std::span<common::NodeInfo*> eligible_nodes) -> std::string = 0;

  using interface_type = Scheduler;
};

} // namespace carrot::gateway
