#pragma once

#include <memory>
#include <string>
#include <vector>

#include <llhttp.h>

#include "carrot/common/extension_registry.hh"
#include "carrot/event/io_object.hh"
#include "carrot/gateway/task_source.hh"

namespace carrot::gateway::task_sources {

class ClientConnection : public event::IOObject {
public:
  ClientConnection(int fd, event::Dispatcher& dispatcher, TaskSource::Handler& handler);
  ~ClientConnection() override;

  void HandleCompletion(int res, uint32_t flags) override;
  void ProcessCommand(event::Command cmd) override;

  auto fd() const { return fd_; }
  void setWriteInFlight(bool close_after) {
    write_in_flight_ = true;
    close_after_write_ = close_after;
  }

private:
  static int onBody(llhttp_t* parser, const char* at, size_t length);
  static int onMessageComplete(llhttp_t* parser);

  int onBodyImpl(const char* at, size_t length);
  int onMessageCompleteImpl();

  int fd_;
  event::Dispatcher& dispatcher_;
  TaskSource::Handler& handler_;
  std::vector<std::byte> read_buf_;
  std::vector<std::byte> body_;
  llhttp_t parser_;
  llhttp_settings_t settings_;
  bool write_in_flight_{false};
  bool close_after_write_{false};
  std::vector<std::byte> response_buf_;
};

class HttpTaskSource : public TaskSource, public event::IOObject {
public:
  HttpTaskSource(common::FactoryContext& ctx, const common::Config& cfg);
  ~HttpTaskSource() override;

  void setHandler(Handler& handler) override { handler_ = &handler; }

  void HandleCompletion(int res, uint32_t flags) override;
  void ProcessCommand(event::Command cmd) override;

private:
  void onAccept(int fd);

  event::Dispatcher& dispatcher_;
  Handler* handler_{nullptr};
  int listen_fd_{-1};
  std::vector<std::unique_ptr<ClientConnection>> connections_;
};

} // namespace carrot::gateway::task_sources
