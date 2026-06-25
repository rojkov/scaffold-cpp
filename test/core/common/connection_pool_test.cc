#include "carrot/common/connection_pool.hh"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace carrot::common {
namespace {

class MockPoolHandler : public ConnectionPool::Handler {
public:
  MOCK_METHOD(void, onTaskResult, (uint64_t, Chunk, bool), (override));
  MOCK_METHOD(void, onTaskError, (uint64_t, std::string_view), (override));
};

auto makeNode(const std::string& id) -> NodeInfo {
  NodeInfo info;
  info.id = id;
  info.address = "127.0.0.1:9090";
  info.task_types = {"echo"};
  info.max_capacity = {{"cpu", 4}, {"mem", 1024}};
  return info;
}

TEST(ConnectionPoolTest, AddAndRemoveNode) {
  MockPoolHandler handler;
  ConnectionPool pool(handler);

  pool.addNode(makeNode("node-1"));
  auto* conn = pool.getConnection("node-1");
  EXPECT_NE(conn, nullptr);

  pool.removeNode("node-1");
  EXPECT_EQ(pool.getConnection("node-1"), nullptr);
}

TEST(ConnectionPoolTest, GetConnectionReturnsNullForUnknown) {
  MockPoolHandler handler;
  ConnectionPool pool(handler);
  EXPECT_EQ(pool.getConnection("nonexistent"), nullptr);
}

TEST(ConnectionPoolTest, IsNotAliveWhenConnectFails) {
  MockPoolHandler handler;
  ConnectionPool pool(handler);
  pool.addNode(makeNode("node-1"));
  EXPECT_FALSE(pool.isAlive("node-1"));
}

TEST(ConnectionPoolTest, MarkDead) {
  MockPoolHandler handler;
  ConnectionPool pool(handler);
  pool.addNode(makeNode("node-1"));
  pool.markDead("node-1");
  EXPECT_FALSE(pool.isAlive("node-1"));
}

TEST(ConnectionPoolTest, HasCapacityWithinLimits) {
  MockPoolHandler handler;
  ConnectionPool pool(handler);
  pool.addNode(makeNode("node-1"));

  Task task;
  task.type = "echo";
  task.requirements = {{"cpu", 2}, {"mem", 512}};

  EXPECT_TRUE(pool.hasCapacity("node-1", task));
}

TEST(ConnectionPoolTest, ReserveAndReleaseCapacity) {
  MockPoolHandler handler;
  ConnectionPool pool(handler);
  pool.addNode(makeNode("node-1"));

  Task task;
  task.type = "echo";
  task.requirements = {{"cpu", 2}, {"mem", 256}};

  pool.reserveCapacity("node-1", task);

  auto* conn = pool.getConnection("node-1");
  ASSERT_NE(conn, nullptr);
  EXPECT_EQ(conn->cpu_used, 2u);
  EXPECT_EQ(conn->mem_used, 256u);

  pool.releaseCapacity("node-1", task);
  EXPECT_EQ(conn->cpu_used, 0u);
  EXPECT_EQ(conn->mem_used, 0u);
}

TEST(ConnectionPoolTest, UpdateNodeReplacesConnection) {
  MockPoolHandler handler;
  ConnectionPool pool(handler);
  pool.addNode(makeNode("node-1"));

  auto updated = makeNode("node-1");
  pool.updateNode(updated);

  auto* conn = pool.getConnection("node-1");
  ASSERT_NE(conn, nullptr);
}

} // namespace
} // namespace carrot::common
