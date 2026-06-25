#include <thread>
#include <unordered_map>
#include <vector>

#include <msgpack.h>

#include <yaml-cpp/yaml.h>

#include "carrot/common/connection_pool.hh"
#include "carrot/common/extension_registry.hh"
#include "carrot/common/node_command.hh"
#include "carrot/common/node_directory.hh"
#include "carrot/common/result_receiver.hh"
#include "carrot/common/types.hh"
#include "carrot/common/wire_protocol.hh"
#include "carrot/event/io_object.hh"
#include "carrot/gateway/node_discovery.hh"
#include "carrot/gateway/scheduler.hh"
#include "carrot/gateway/task_source.hh"
#include "core/common/signal_monitor.hh"
#include "core/event/dispatcher_impl.hh"
#include "core/logging/log.hh"

class Worker : public carrot::gateway::TaskSource::Handler,
               public carrot::common::ConnectionPool::Handler,
               public carrot::event::IOObject {
public:
  Worker(const YAML::Node& cfg, carrot::common::NodeDirectory initial_dir)
      : dispatcher_(4096),
        node_dir_(std::move(initial_dir)),
        conn_pool_(*this) {
    auto& reg = carrot::common::ExtensionRegistry::getInstance();

    {
      auto task_src_cfg = cfg["task_source"];
      auto task_source_name = task_src_cfg["type"].as<std::string>();
      carrot::common::FactoryContext ctx(dispatcher_);
      task_source_ = reg.create<carrot::gateway::TaskSource>("task_source", task_source_name, ctx,
                                                              task_src_cfg);
      task_source_->setHandler(*this);
    }

    {
      auto sched_cfg = cfg["scheduler"];
      auto sched_name = sched_cfg["type"].as<std::string>();
      carrot::common::FactoryContext ctx(dispatcher_);
      scheduler_ =
          reg.create<carrot::gateway::Scheduler>("scheduler", sched_name, ctx, sched_cfg);
    }
  }

  void run() { dispatcher_.Run(); }

  void applyCommand(const carrot::common::NodeCommand& cmd) {
    std::visit(
        [this](const auto& c) {
          using T = std::decay_t<decltype(c)>;
          if constexpr (std::is_same_v<T, carrot::common::AddNode>) {
            node_dir_.addNode(c.info);
            conn_pool_.addNode(c.info);
          } else if constexpr (std::is_same_v<T, carrot::common::RemoveNode>) {
            node_dir_.removeNode(c.node_id);
            conn_pool_.removeNode(c.node_id);
          } else if constexpr (std::is_same_v<T, carrot::common::UpdateNode>) {
            node_dir_.updateNode(c.info);
            conn_pool_.updateNode(c.info);
          }
        },
        cmd);
  }

  auto dispatcher() -> carrot::event::DispatcherImpl& { return dispatcher_; }

  // TaskSource::Handler
  void onTaskReady(carrot::common::Task task,
                   std::unique_ptr<carrot::common::ResultReceiver> receiver) override {
    auto eligible = node_dir_.byType(task.type);
    auto node_id = scheduler_->selectNode(task, eligible);
    if (node_id.empty()) {
      receiver->sendChunk({}, true);
      return;
    }

    pending_receivers_[task_index_] = std::move(receiver);
    conn_pool_.reserveCapacity(node_id, task);
    auto task_id = task_index_++;
    auto task_body = carrot::common::wire::serializeTask(task);
    auto header = carrot::common::wire::encodeHeader(
        carrot::common::wire::MessageType::kSubmitTask, task_body.size());

    auto* conn = conn_pool_.getConnection(node_id);
    if (conn != nullptr && conn->alive) {
      dispatcher_.PrepareWrite(this, conn->fd, std::as_bytes(std::span(header)), 0);
      dispatcher_.PrepareWrite(this, conn->fd, std::as_bytes(std::span(task_body)), 0);

      auto read_ctx = std::make_unique<NodeReadContext>(conn->fd, this);
      dispatcher_.PrepareRead(read_ctx.get(), conn->fd, read_ctx->buf, 0);
      node_reads_[conn->fd] = std::move(read_ctx);
    } else {
      pending_receivers_.erase(task_id);
      receiver->sendChunk({}, true);
    }
  }

  // ConnectionPool::Handler
  void onTaskResult(uint64_t task_id, carrot::common::Chunk chunk, bool is_final) override {
    auto it = pending_receivers_.find(task_id);
    if (it != pending_receivers_.end()) {
      it->second->sendChunk(std::move(chunk), is_final);
      if (is_final) {
        pending_receivers_.erase(it);
      }
    }
  }

  void onTaskError(uint64_t task_id, std::string_view /*message*/) override {
    auto it = pending_receivers_.find(task_id);
    if (it != pending_receivers_.end()) {
      it->second->sendChunk({}, true);
      pending_receivers_.erase(it);
    }
  }

  // IOObject
  void HandleCompletion(int /*res*/, uint32_t /*flags*/) override {}
  void ProcessCommand(carrot::event::Command /*cmd*/) override {}

  void onNodeReadComplete(int fd, std::vector<std::byte>& buf) {
    auto header = carrot::common::wire::decodeHeader(buf);
    auto payload_start = std::span(buf).subspan(5, header.length);

    uint64_t task_id = 0;
    if (header.type == carrot::common::wire::MessageType::kChunk ||
        header.type == carrot::common::wire::MessageType::kComplete) {
      msgpack_unpacked msg;
      msgpack_unpacked_init(&msg);
      size_t off = 0;
      msgpack_unpack_next(&msg,
                          reinterpret_cast<const char*>(payload_start.data()),
                          payload_start.size(), &off);
      auto obj = msg.data;
      if (obj.type == MSGPACK_OBJECT_MAP) {
        for (uint32_t i = 0; i < obj.via.map.size; ++i) {
          auto key = obj.via.map.ptr[i].key;
          auto val = obj.via.map.ptr[i].val;
          if (key.type == MSGPACK_OBJECT_STR &&
              std::string_view(key.via.str.ptr, key.via.str.size) == "task_id" &&
              val.type == MSGPACK_OBJECT_STR) {
            task_id = std::stoul(std::string(val.via.str.ptr, val.via.str.size));
            break;
          }
        }
      }
      msgpack_unpacked_destroy(&msg);
    }

    switch (header.type) {
      case carrot::common::wire::MessageType::kChunk: {
        msgpack_unpacked msg;
        msgpack_unpacked_init(&msg);
        size_t off = 0;
        msgpack_unpack_next(&msg,
                            reinterpret_cast<const char*>(payload_start.data()),
                            payload_start.size(), &off);
        auto obj = msg.data;
        carrot::common::Chunk chunk;
        bool is_final = false;
        if (obj.type == MSGPACK_OBJECT_MAP) {
          for (uint32_t i = 0; i < obj.via.map.size; ++i) {
            auto key = obj.via.map.ptr[i].key;
            auto val = obj.via.map.ptr[i].val;
            if (key.type != MSGPACK_OBJECT_STR) continue;
            auto ks = std::string_view(key.via.str.ptr, key.via.str.size);
            if (ks == "data" && val.type == MSGPACK_OBJECT_BIN) {
              chunk.assign(
                  reinterpret_cast<const std::byte*>(val.via.bin.ptr),
                  reinterpret_cast<const std::byte*>(val.via.bin.ptr) + val.via.bin.size);
            } else if (ks == "is_final" && val.type == MSGPACK_OBJECT_BOOLEAN) {
              is_final = val.via.boolean;
            }
          }
        }
        msgpack_unpacked_destroy(&msg);
        onTaskResult(task_id, std::move(chunk), is_final);
        break;
      }
      case carrot::common::wire::MessageType::kComplete: {
        onTaskResult(task_id, {}, true);
        break;
      }
      case carrot::common::wire::MessageType::kError: {
        onTaskError(task_id, {});
        break;
      }
      default:
        break;
    }
  }

private:
  struct NodeReadContext : public carrot::event::IOObject {
    int fd;
    Worker* worker;
    std::vector<std::byte> buf;

    NodeReadContext(int fd, Worker* worker) : fd(fd), worker(worker) {
      buf.resize(4096);
    }

    void HandleCompletion(int res, uint32_t /*flags*/) override {
      if (res <= 0) return;
      worker->onNodeReadComplete(fd, buf);
      worker->dispatcher_.PrepareRead(this, fd, buf, 0);
    }

    void ProcessCommand(carrot::event::Command /*cmd*/) override {}
  };

  carrot::event::DispatcherImpl dispatcher_;
  carrot::common::NodeDirectory node_dir_;
  carrot::common::ConnectionPool conn_pool_;
  std::unique_ptr<carrot::gateway::TaskSource> task_source_;
  std::unique_ptr<carrot::gateway::Scheduler> scheduler_;
  uint64_t task_index_{0};
  std::unordered_map<uint64_t, std::unique_ptr<carrot::common::ResultReceiver>>
      pending_receivers_;
  std::unordered_map<int, std::unique_ptr<NodeReadContext>> node_reads_;
};

auto main() -> int {
  auto config = YAML::LoadFile("gateway.yaml");

  auto& logger = carrot::logging::Logger::GetInstance();
  logger.Run();
  LOG_REGISTER_THREAD();

  carrot::common::NodeDirectory initial_dir;

  carrot::event::DispatcherImpl main_dispatcher(64);
  carrot::common::SignalMonitor signal_monitor(
      std::shared_ptr<carrot::event::Dispatcher>(&main_dispatcher, [](auto*) {}));

  auto worker_count = config["workers"].as<uint32_t>(1);
  std::vector<std::unique_ptr<Worker>> workers;
  std::vector<std::thread> worker_threads;

  for (uint32_t i = 0; i < worker_count; ++i) {
    auto worker = std::make_unique<Worker>(config, carrot::common::NodeDirectory{});
    worker_threads.emplace_back([w = worker.get()] { w->run(); });
    workers.push_back(std::move(worker));
  }

  LOG_INFO("gateway started with {} workers", worker_count);

  auto& reg = carrot::common::ExtensionRegistry::getInstance();
  {
    auto discovery_cfg = config["node_discovery"];
    auto discovery_name = discovery_cfg["type"].as<std::string>();
    carrot::common::FactoryContext ctx(main_dispatcher);
    auto discovery = reg.create<carrot::gateway::NodeDiscovery>("node_discovery", discovery_name,
                                                                ctx, discovery_cfg);

    struct DiscoveryHandler : carrot::gateway::NodeDiscovery::Handler {
      std::vector<std::unique_ptr<Worker>>& workers_;
      explicit DiscoveryHandler(std::vector<std::unique_ptr<Worker>>& workers) : workers_(workers) {}
      void onNodeAdded(const carrot::common::NodeInfo& info) override {
        carrot::common::NodeCommand cmd = carrot::common::AddNode{info};
        for (auto& w : workers_) {
          w->applyCommand(cmd);
        }
      }
      void onNodeRemoved(std::string_view node_id) override {
        carrot::common::NodeCommand cmd = carrot::common::RemoveNode{std::string(node_id)};
        for (auto& w : workers_) {
          w->applyCommand(cmd);
        }
      }
      void onNodeUpdated(const carrot::common::NodeInfo& info) override {
        carrot::common::NodeCommand cmd = carrot::common::UpdateNode{info};
        for (auto& w : workers_) {
          w->applyCommand(cmd);
        }
      }
    };
    DiscoveryHandler dh{workers};
    discovery->setHandler(dh);
  }

  main_dispatcher.Run();

  for (auto& t : worker_threads) {
    if (t.joinable()) {
      t.join();
    }
  }

  logger.Stop();
  return 0;
}
