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

#include "carrot/common/result_receiver.hh"
#include "carrot/event/dispatcher.hh"

namespace carrot::gateway::task_sources {

ClientConnection::ClientConnection(int fd, event::Dispatcher& dispatcher,
                                   TaskSource::Handler& handler)
    : fd_(fd), dispatcher_(dispatcher), handler_(handler) {
  read_buf_.resize(4096);
  body_.clear();
  llhttp_settings_init(&settings_);
  settings_.on_body = onBody;
  settings_.on_message_complete = onMessageComplete;
  llhttp_init(&parser_, HTTP_REQUEST, &settings_);
  parser_.data = this;
  dispatcher_.PrepareRead(this, fd_, read_buf_, 0);
}

ClientConnection::~ClientConnection() {
  close(fd_);
}

void ClientConnection::HandleCompletion(int res, uint32_t /*flags*/) {
  if (write_in_flight_) {
    write_in_flight_ = false;
    if (close_after_write_) {
      close(fd_);
    }
    return;
  }
  if (res <= 0) {
    return;
  }
  auto err = llhttp_execute(&parser_,
                             reinterpret_cast<const char*>(read_buf_.data()),
                             static_cast<size_t>(res));
  if (err != HPE_OK) {
    return;
  }
  dispatcher_.PrepareRead(this, fd_, read_buf_, 0);
}

void ClientConnection::ProcessCommand(event::Command cmd) {
  if (cmd.type_ == event::Command::CLOSE_CONNECTION) {
    close(fd_);
  }
}

int ClientConnection::onBody(llhttp_t* parser, const char* at, size_t length) {
  auto* self = static_cast<ClientConnection*>(parser->data);
  return self->onBodyImpl(at, length);
}

int ClientConnection::onMessageComplete(llhttp_t* parser) {
  auto* self = static_cast<ClientConnection*>(parser->data);
  return self->onMessageCompleteImpl();
}

int ClientConnection::onBodyImpl(const char* at, size_t length) {
  auto bytes = std::as_bytes(std::span<const char>{at, length});
  body_.insert(body_.end(), bytes.begin(), bytes.end());
  return 0;
}

int ClientConnection::onMessageCompleteImpl() {
  class HttpResultReceiver : public common::ResultReceiver {
  public:
    HttpResultReceiver(int fd, event::Dispatcher& d, ClientConnection* owner)
        : fd_(fd), dispatcher_(d), owner_(owner) {}
    void sendChunk(common::Chunk chunk, bool is_final) override {
      auto response = std::format(
          "HTTP/1.1 200 OK\r\nContent-Length: {}\r\nContent-Type: "
          "text/plain\r\nConnection: close\r\n\r\n{}",
          chunk.size(),
          std::string_view(reinterpret_cast<const char*>(chunk.data()), chunk.size()));
      owner_->response_buf_.assign(
          reinterpret_cast<const std::byte*>(response.data()),
          reinterpret_cast<const std::byte*>(response.data() + response.size()));
      owner_->setWriteInFlight(is_final);
      dispatcher_.PrepareWrite(owner_, fd_,
                                std::as_bytes(std::span(owner_->response_buf_)), 0);
    }

  private:
    int fd_;
    event::Dispatcher& dispatcher_;
    ClientConnection* owner_;
  };

  handler_.onTaskReady(
      common::Task{
          .type = "echo",
          .body = std::move(body_),
      },
      std::make_unique<HttpResultReceiver>(fd_, dispatcher_, this));
  return 0;
}

HttpTaskSource::HttpTaskSource(common::FactoryContext& ctx, const common::Config& cfg)
    : dispatcher_(ctx.dispatcher()) {
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

  dispatcher_.PrepareAcceptMultishot(this, listen_fd_);
}

HttpTaskSource::~HttpTaskSource() {
  close(listen_fd_);
}

void HttpTaskSource::HandleCompletion(int res, uint32_t flags) {
  if (res < 0) {
    return;
  }
  onAccept(res);

  if (!(flags & IORING_CQE_F_MORE)) {
    dispatcher_.PrepareAcceptMultishot(this, listen_fd_);
  }
}

void HttpTaskSource::ProcessCommand(event::Command cmd) {
  if (cmd.type_ == event::Command::CLOSE_CONNECTION) {
    auto* fd = static_cast<int*>(cmd.args_);
    auto it = std::find_if(connections_.begin(), connections_.end(),
                           [fd](const auto& conn) { return conn->fd() == *fd; });
    if (it != connections_.end()) {
      connections_.erase(it);
    }
  }
}

void HttpTaskSource::onAccept(int fd) {
  auto conn = std::make_unique<ClientConnection>(fd, dispatcher_, *handler_);
  connections_.push_back(std::move(conn));
}

} // namespace carrot::gateway::task_sources

CARROT_REGISTER_EXTENSION(task_source, "http", carrot::gateway::task_sources::HttpTaskSource);
