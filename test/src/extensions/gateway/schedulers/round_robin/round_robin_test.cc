#include "carrot/common/extension_registry.hh"
#include "carrot/common/types.hh"
#include "carrot/gateway/scheduler.hh"
#include "carrot/event/dispatcher.hh"
#include "extensions/gateway/schedulers/round_robin/round_robin.hh"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace carrot::gateway::schedulers {
namespace {

class StubDispatcher : public event::Dispatcher {
public:
  void Run() override {}
  void Shutdown() override {}
  void SubmitCommand(event::Command) override {}
  void PrepareAcceptMultishot(event::IOObject*, int) override {}
  void PrepareRead(event::IOObject*, int, std::span<std::byte>, off_t) override {}
  void PrepareWrite(event::IOObject*, int, std::span<const std::byte>, off_t) override {}
};

auto makeNode(const std::string& id) -> common::NodeInfo {
  common::NodeInfo info;
  info.id = id;
  info.task_types = {"echo"};
  return info;
}

class RoundRobinTest : public ::testing::Test {
protected:
  StubDispatcher dispatcher;
  common::FactoryContext ctx{dispatcher};
};

TEST_F(RoundRobinTest, SelectsOnlyEligibleNode) {
  auto n1 = makeNode("node-1");
  std::vector<common::NodeInfo*> eligible{&n1};
  common::Task task;
  task.type = "echo";

  RoundRobin sched(ctx, {});
  auto chosen = sched.selectNode(task, eligible);
  EXPECT_EQ(chosen, "node-1");
}

TEST_F(RoundRobinTest, CyclesThroughNodes) {
  auto n1 = makeNode("node-1");
  auto n2 = makeNode("node-2");
  std::vector<common::NodeInfo*> eligible{&n1, &n2};
  common::Task task;
  task.type = "echo";

  RoundRobin sched(ctx, {});
  EXPECT_EQ(sched.selectNode(task, eligible), "node-1");
  EXPECT_EQ(sched.selectNode(task, eligible), "node-2");
  EXPECT_EQ(sched.selectNode(task, eligible), "node-1");
}

TEST_F(RoundRobinTest, ReturnsEmptyOnNoEligibleNodes) {
  std::vector<common::NodeInfo*> eligible;
  common::Task task;
  task.type = "echo";

  RoundRobin sched(ctx, {});
  auto chosen = sched.selectNode(task, eligible);
  EXPECT_TRUE(chosen.empty());
}

TEST_F(RoundRobinTest, SeparateCursorsPerTaskType) {
  auto n1 = makeNode("node-1");
  auto n2 = makeNode("node-2");
  std::vector<common::NodeInfo*> eligible{&n1, &n2};

  RoundRobin sched(ctx, {});
  common::Task task_a;
  task_a.type = "type_a";
  common::Task task_b;
  task_b.type = "type_b";

  EXPECT_EQ(sched.selectNode(task_a, eligible), "node-1");
  EXPECT_EQ(sched.selectNode(task_b, eligible), "node-1");
  EXPECT_EQ(sched.selectNode(task_a, eligible), "node-2");
  EXPECT_EQ(sched.selectNode(task_b, eligible), "node-2");
}

} // namespace
} // namespace carrot::gateway::schedulers
