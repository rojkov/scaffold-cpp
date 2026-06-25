#include <arpa/inet.h>
#include <cstdint>
#include <memory>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>

#include <linux/io_uring.h>
#include <yaml-cpp/yaml.h>

#include "carrot/common/extension_registry.hh"
#include "carrot/common/result_receiver.hh"
#include "carrot/common/types.hh"
#include "carrot/common/wire_protocol.hh"
#include "carrot/event/dispatcher.hh"
#include "carrot/event/io_object.hh"
#include "carrot/nodeagent/task_handler.hh"
#include "core/event/dispatcher_impl.hh"
#include "core/logging/log.hh"

namespace carrot::nodeagent {

class NodeAgentConnection : public event::IOObject {
public:
  NodeAgentConnection(int fd, event::Dispatcher& dispatcher)
      : fd_(fd), dispatcher_(dispatcher) {}

  void HandleCompletion(int res, uint32_t /*flags*/) override {
    if (res <= 0) {
      return;
    }
    auto msg_type = common::wire::decodeHeader(read_buf_);
    auto payload_start = std::span(read_buf_).subspan(5, msg_type.length);

    switch (msg_type.type) {
      case common::wire::MessageType::kSubmitTask: {
        auto task = common::wire::deserializeTask(payload_start);
        handleTask(task);
        break;
      }
      default:
        break;
    }

    read_buf_.clear();
    read_buf_.resize(4096);
    dispatcher_.PrepareRead(this, fd_, read_buf_, 0);
  }

  void ProcessCommand(event::Command /*cmd*/) override {}

  void startRead() {
    read_buf_.resize(4096);
    dispatcher_.PrepareRead(this, fd_, read_buf_, 0);
  }

private:
  void handleTask(const common::Task& task) {
    auto& reg = common::ExtensionRegistry::getInstance();
    common::FactoryContext ctx(dispatcher_);
    auto handler = reg.create<carrot::nodeagent::TaskHandler>("task_handler", task.type, ctx, {});

    if (!handler) {
      chunk_data_ = common::wire::serializeError(task_id_,
                                                  "no handler for task type: " + task.type);
      chunk_header_ = common::wire::encodeHeader(common::wire::MessageType::kError,
                                                  chunk_data_.size());
      dispatcher_.PrepareWrite(this, fd_, std::as_bytes(std::span(chunk_header_)), 0);
      dispatcher_.PrepareWrite(this, fd_, std::as_bytes(std::span(chunk_data_)), 0);
      return;
    }

    class NodeResultReceiver : public common::ResultReceiver {
    public:
      NodeResultReceiver(uint64_t task_id, NodeAgentConnection* owner)
          : task_id_(task_id), owner_(owner) {}

      void sendChunk(common::Chunk chunk, bool is_final) override {
        if (!chunk.empty()) {
          owner_->chunk_data_ = common::wire::serializeChunk(task_id_, std::move(chunk), is_final);
          owner_->chunk_header_ =
              common::wire::encodeHeader(common::wire::MessageType::kChunk,
                                         owner_->chunk_data_.size());
          owner_->dispatcher_.PrepareWrite(owner_, owner_->fd_,
                                           std::as_bytes(std::span(owner_->chunk_header_)), 0);
          owner_->dispatcher_.PrepareWrite(owner_, owner_->fd_,
                                           std::as_bytes(std::span(owner_->chunk_data_)), 0);
        }
        if (is_final) {
          owner_->complete_data_ = common::wire::serializeComplete(task_id_);
          owner_->complete_header_ =
              common::wire::encodeHeader(common::wire::MessageType::kComplete,
                                         owner_->complete_data_.size());
          owner_->dispatcher_.PrepareWrite(owner_, owner_->fd_,
                                           std::as_bytes(std::span(owner_->complete_header_)), 0);
          owner_->dispatcher_.PrepareWrite(owner_, owner_->fd_,
                                           std::as_bytes(std::span(owner_->complete_data_)), 0);
        }
      }

    private:
      uint64_t task_id_;
      NodeAgentConnection* owner_;
    };

    auto receiver = std::make_unique<NodeResultReceiver>(task_id_++, this);
    handler->handleTask(task, *receiver);
  }

  int fd_;
  event::Dispatcher& dispatcher_;
  std::vector<std::byte> read_buf_;
  uint64_t task_id_{0};
  std::array<std::byte, 5> chunk_header_{};
  std::vector<std::byte> chunk_data_;
  std::array<std::byte, 5> complete_header_{};
  std::vector<std::byte> complete_data_;
};

class NodeAgentListener : public event::IOObject {
public:
  NodeAgentListener(event::Dispatcher& dispatcher, uint32_t port)
      : dispatcher_(dispatcher) {
    fd_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    struct sockaddr_in addr = {.sin_family = AF_INET,
                               .sin_port = htons(static_cast<uint16_t>(port))};
    bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    listen(fd_, SOMAXCONN);
    dispatcher_.PrepareAcceptMultishot(this, fd_);
  }

  void HandleCompletion(int res, uint32_t flags) override {
    if (res < 0) {
      return;
    }
    auto conn = std::make_unique<NodeAgentConnection>(res, dispatcher_);
    conn->startRead();
    connections_.push_back(std::move(conn));

    if (!(flags & IORING_CQE_F_MORE)) {
      dispatcher_.PrepareAcceptMultishot(this, fd_);
    }
  }

  void ProcessCommand(event::Command /*cmd*/) override {}

private:
  event::Dispatcher& dispatcher_;
  int fd_;
  std::vector<std::unique_ptr<NodeAgentConnection>> connections_;
};

} // namespace carrot::nodeagent

auto main() -> int {
  auto config = YAML::LoadFile("nodeagent.yaml");
  auto port = config["port"].as<uint32_t>(8082);

  auto& logger = carrot::logging::Logger::GetInstance();
  logger.Run();
  LOG_REGISTER_THREAD();

  carrot::event::DispatcherImpl dispatcher(4096);
  carrot::nodeagent::NodeAgentListener listener(dispatcher, port);

  LOG_INFO("node agent listening on port {}", port);
  dispatcher.Run();
  logger.Stop();

  return 0;
}
