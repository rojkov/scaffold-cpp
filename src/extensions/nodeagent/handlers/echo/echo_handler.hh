#pragma once

#include "carrot/common/extension_registry.hh"
#include "carrot/nodeagent/task_handler.hh"

namespace carrot::nodeagent::handlers {

class Echo : public TaskHandler {
public:
  explicit Echo(common::FactoryContext&, const common::Config&) {}

  void handleTask(const common::Task& task, common::ResultReceiver& receiver) override;
};

} // namespace carrot::nodeagent::handlers
