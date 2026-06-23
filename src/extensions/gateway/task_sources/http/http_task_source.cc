#include "http_task_source.hh"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <any>
#include <cstring>
#include <format>
#include <string_view>

#include <linux/io_uring.h>

#include "carrot/event/dispatcher.hh"

namespace carrot::gateway::task_sources {

HttpTaskSource::HttpTaskSource(common::FactoryContext& ctx, const common::Config& cfg)
    : ctx_(ctx) {
  auto port = std::any_cast<uint32_t>(&cfg);
  uint32_t actual_port = port ? *port : 8081;

  listen_fd_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
  if (listen_fd_ < 0) {
    throw std::runtime_error("unable to open TCP socket");
  }

  int opt = 1;
  setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

  struct sockaddr_in addr = {.sin_family = AF_INET,
                             .sin_port = htons(static_cast<uint16_t>(actual_port))};
  if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    close(listen_fd_);
    throw std::runtime_error(std::format("unable to bind to port {}", actual_port));
  }

  if (listen(listen_fd_, SOMAXCONN) < 0) {
    close(listen_fd_);
    throw std::runtime_error("unable to listen");
  }

  ctx_.dispatcher().PrepareAcceptMultishot(this, listen_fd_);
}

void HttpTaskSource::HandleCompletion(int res, uint32_t flags) {
  if (res < 0) {
    return;
  }
  onAccept(res);

  if (!(flags & IORING_CQE_F_MORE)) {
    ctx_.dispatcher().PrepareAcceptMultishot(this, listen_fd_);
  }
}

void HttpTaskSource::ProcessCommand(event::Command cmd) {
  if (cmd.type_ == event::Command::CLOSE_CONNECTION) {
    auto* fd = static_cast<int*>(cmd.args_);
    auto it = std::find(client_fds_.begin(), client_fds_.end(), *fd);
    if (it != client_fds_.end()) {
      close(*it);
      client_fds_.erase(it);
    }
  }
}

void HttpTaskSource::onAccept(int fd) {
  client_fds_.push_back(fd);
  ctx_.dispatcher().PrepareRead(this, fd, {}, 0);
}

void HttpTaskSource::onRequest(int client_fd, std::string body) {
  common::Task task;
  task.type = "echo";
  task.body.assign(reinterpret_cast<const std::byte*>(body.data()),
                   reinterpret_cast<const std::byte*>(body.data() + body.size()));

  class HttpResultReceiver : public common::ResultReceiver {
  public:
    HttpResultReceiver(int fd, event::Dispatcher& d, event::IOObject* owner)
        : fd_(fd), dispatcher_(d), owner_(owner) {}
    void sendChunk(common::Chunk chunk, bool is_final) override {
      auto response = std::format(
          "HTTP/1.1 200 OK\r\nContent-Length: {}\r\nContent-Type: "
          "text/plain\r\nConnection: close\r\n\r\n{}",
          chunk.size(),
          std::string_view(reinterpret_cast<const char*>(chunk.data()), chunk.size()));
      auto buf = std::vector<std::byte>(reinterpret_cast<const std::byte*>(response.data()),
                                        reinterpret_cast<const std::byte*>(response.data() +
                                                                           response.size()));
      dispatcher_.PrepareWrite(owner_, fd_, std::as_bytes(std::span(buf)), 0);
      if (is_final) {
        close(fd_);
      }
    }

  private:
    int fd_;
    event::Dispatcher& dispatcher_;
    event::IOObject* owner_;
  };

  if (handler_) {
    handler_->onTaskReady(std::move(task),
                          std::make_unique<HttpResultReceiver>(client_fd, ctx_.dispatcher(), this));
  }
}

} // namespace carrot::gateway::task_sources

CARROT_REGISTER_EXTENSION(task_source, "http", carrot::gateway::task_sources::HttpTaskSource);
