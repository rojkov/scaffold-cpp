#include "echo_handler.hh"

namespace carrot::nodeagent::handlers {

void Echo::handleTask(const common::Task& task, common::ResultReceiver& receiver) {
  receiver.sendChunk(task.body, true);
}

} // namespace carrot::nodeagent::handlers

CARROT_REGISTER_EXTENSION(task_handler, "echo", carrot::nodeagent::handlers::Echo);
