#include <memory>
#include <vector>

#include "carrot/common/extension_registry.hh"
#include "carrot/common/node_directory.hh"
#include "carrot/common/types.hh"
#include "carrot/common/wire_protocol.hh"
#include "carrot/event/dispatcher.hh"
#include "carrot/gateway/scheduler.hh"
#include "carrot/nodeagent/task_handler.hh"
#include "extensions/gateway/schedulers/round_robin/round_robin.hh"
#include "extensions/nodeagent/handlers/echo/echo_handler.hh"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace carrot::common {
namespace {

using ::testing::_;
using ::testing::Eq;

class StubDispatcher : public event::Dispatcher {
public:
  void Run() override {}
  void Shutdown() override {}
  void SubmitCommand(event::Command) override {}
  void PrepareAcceptMultishot(event::IOObject*, int) override {}
  void PrepareRead(event::IOObject*, int, std::span<std::byte>, off_t) override {}
  void PrepareWrite(event::IOObject*, int, std::span<const std::byte>, off_t) override {}
};

class MockResultReceiver : public ResultReceiver {
public:
  MOCK_METHOD(void, sendChunk, (Chunk chunk, bool is_final), (override));
};

class EchoRoundtripTest : public ::testing::Test {
protected:
  void SetUp() override {
    ctx_ = std::make_unique<FactoryContext>(dispatcher_);

    auto& reg = ExtensionRegistry::getInstance();
    reg.addFactory<gateway::Scheduler>(
        "scheduler", "round_robin",
        [this](FactoryContext&, const Config&) -> std::unique_ptr<gateway::Scheduler> {
          return std::make_unique<gateway::schedulers::RoundRobin>(*ctx_, Config{});
        });
    reg.addFactory<nodeagent::TaskHandler>(
        "task_handler", "echo",
        [this](FactoryContext&, const Config&) -> std::unique_ptr<nodeagent::TaskHandler> {
          return std::make_unique<nodeagent::handlers::Echo>(*ctx_, Config{});
        });

    NodeInfo node;
    node.id = "echo-node";
    node.address = "127.0.0.1:9090";
    node.task_types = {"echo"};
    node.max_capacity = {{"cpu", 4}, {"mem", 1024}};
    directory_.addNode(std::move(node));
  }

  StubDispatcher dispatcher_;
  std::unique_ptr<FactoryContext> ctx_;
  NodeDirectory directory_;
};

TEST_F(EchoRoundtripTest, EchoTaskRoundtrip) {
  // 1. Gateway side: create task
  Task task;
  task.type = "echo";
  task.body = {std::byte{0xDE}, std::byte{0xAD}, std::byte{0xBE}, std::byte{0xEF}};

  // 2. Gateway side: schedule
  auto eligible = directory_.byType(task.type);
  ASSERT_EQ(eligible.size(), 1u);
  auto& reg = ExtensionRegistry::getInstance();
  auto scheduler = reg.create<gateway::Scheduler>("scheduler", "round_robin", *ctx_, Config{});
  ASSERT_NE(scheduler, nullptr);
  auto node_id = scheduler->selectNode(task, eligible);
  EXPECT_EQ(node_id, "echo-node");

  // 3. Gateway side: serialize via wire protocol
  auto wire_data = wire::serializeTask(task);
  ASSERT_FALSE(wire_data.empty());

  // 4. Node agent side: deserialize
  auto received_task = wire::deserializeTask(wire_data);
  EXPECT_EQ(received_task.type, task.type);
  EXPECT_EQ(received_task.body, task.body);

  // 5. Node agent side: handle with Echo handler
  auto handler =
      reg.create<nodeagent::TaskHandler>("task_handler", "echo", *ctx_, Config{});
  ASSERT_NE(handler, nullptr);

  MockResultReceiver mock_receiver;
  EXPECT_CALL(mock_receiver, sendChunk(_, /*is_final=*/true))
      .WillOnce([&task](Chunk chunk, bool) {
        EXPECT_EQ(chunk, task.body);
      });

  handler->handleTask(received_task, mock_receiver);

  // 6. Gateway side: receive result via wire protocol
  Chunk result_chunk = task.body;
  auto result_wire = wire::serializeChunk(0, result_chunk, true);
  ASSERT_FALSE(result_wire.empty());

  auto header = wire::decodeHeader(wire::encodeHeader(wire::MessageType::kChunk, result_wire.size()));
  EXPECT_EQ(header.type, wire::MessageType::kChunk);
  EXPECT_EQ(header.length, result_wire.size());
}

} // namespace
} // namespace carrot::common
