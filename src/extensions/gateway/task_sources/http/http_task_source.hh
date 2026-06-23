#pragma once

#include <memory>
#include <string>
#include <vector>

#include "carrot/common/extension_registry.hh"
#include "carrot/event/io_object.hh"
#include "carrot/gateway/task_source.hh"

namespace carrot::gateway::task_sources {

class HttpTaskSource : public TaskSource, public event::IOObject {
public:
  HttpTaskSource(common::FactoryContext& ctx, const common::Config& cfg);

  void setHandler(Handler& handler) override { handler_ = &handler; }

  void HandleCompletion(int res, uint32_t flags) override;
  void ProcessCommand(event::Command cmd) override;

private:
  void onAccept(int fd);
  void onRequest(int client_fd, std::string body);

  common::FactoryContext& ctx_;
  Handler* handler_{nullptr};
  int listen_fd_{-1};
  std::vector<int> client_fds_;
};

} // namespace carrot::gateway::task_sources
