#pragma once

#include "carrot/common/result_receiver.hh"
#include "carrot/common/types.hh"

namespace carrot::nodeagent {

class TaskHandler {
public:
  TaskHandler() = default;
  virtual ~TaskHandler() = default;

  TaskHandler(const TaskHandler&) = delete;
  auto operator=(const TaskHandler&) -> TaskHandler& = delete;
  TaskHandler(TaskHandler&&) noexcept = delete;
  auto operator=(TaskHandler&&) noexcept -> TaskHandler& = delete;

  virtual void handleTask(const common::Task& task, common::ResultReceiver& receiver) = 0;

  using interface_type = TaskHandler;
};

} // namespace carrot::nodeagent
