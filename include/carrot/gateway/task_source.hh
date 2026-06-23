#pragma once

#include <functional>
#include <memory>

#include "carrot/common/result_receiver.hh"
#include "carrot/common/types.hh"

namespace carrot::gateway {

class TaskSource {
public:
  struct Handler {
    virtual void onTaskReady(common::Task task,
                             std::unique_ptr<common::ResultReceiver> receiver) = 0;
    virtual ~Handler() = default;
  };

  TaskSource() = default;
  virtual ~TaskSource() = default;

  TaskSource(const TaskSource&) = delete;
  auto operator=(const TaskSource&) -> TaskSource& = delete;
  TaskSource(TaskSource&&) noexcept = delete;
  auto operator=(TaskSource&&) noexcept -> TaskSource& = delete;

  virtual void setHandler(Handler& handler) = 0;

  using interface_type = TaskSource;
};

} // namespace carrot::gateway
