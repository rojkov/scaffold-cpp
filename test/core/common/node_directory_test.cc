#include "carrot/common/node_directory.hh"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace carrot::common {
namespace {

auto makeNode(const std::string& id, const std::string& type) -> NodeInfo {
  NodeInfo info;
  info.id = id;
  info.address = "127.0.0.1:8080";
  info.task_types = {type};
  info.max_capacity = {{"cpu", 4}, {"mem", 1024}};
  return info;
}

TEST(NodeDirectoryTest, AddAndFindById) {
  NodeDirectory dir;
  dir.addNode(makeNode("node-1", "echo"));

  auto* found = dir.byId("node-1");
  ASSERT_NE(found, nullptr);
  EXPECT_EQ(found->id, "node-1");
}

TEST(NodeDirectoryTest, ByIdReturnsNullForUnknown) {
  NodeDirectory dir;
  EXPECT_EQ(dir.byId("nonexistent"), nullptr);
}

TEST(NodeDirectoryTest, FindByType) {
  NodeDirectory dir;
  dir.addNode(makeNode("node-1", "echo"));
  dir.addNode(makeNode("node-2", "echo"));

  auto nodes = dir.byType("echo");
  EXPECT_EQ(nodes.size(), 2u);

  auto nodes_wrong = dir.byType("compute");
  EXPECT_TRUE(nodes_wrong.empty());
}

TEST(NodeDirectoryTest, RemoveNode) {
  NodeDirectory dir;
  dir.addNode(makeNode("node-1", "echo"));
  dir.addNode(makeNode("node-2", "echo"));

  dir.removeNode("node-1");
  EXPECT_EQ(dir.byId("node-1"), nullptr);
  EXPECT_EQ(dir.byType("echo").size(), 1u);
}

TEST(NodeDirectoryTest, UpdateNode) {
  NodeDirectory dir;
  dir.addNode(makeNode("node-1", "echo"));

  auto updated = makeNode("node-1", "compute");
  dir.updateNode(updated);

  auto* found = dir.byId("node-1");
  ASSERT_NE(found, nullptr);
  EXPECT_EQ(found->task_types[0], "compute");
  EXPECT_TRUE(dir.byType("echo").empty());
  EXPECT_EQ(dir.byType("compute").size(), 1u);
}

TEST(NodeDirectoryTest, MultipleTypesPerNode) {
  NodeDirectory dir;
  NodeInfo info;
  info.id = "node-1";
  info.task_types = {"echo", "compute"};
  dir.addNode(std::move(info));

  EXPECT_EQ(dir.byType("echo").size(), 1u);
  EXPECT_EQ(dir.byType("compute").size(), 1u);
}

} // namespace
} // namespace carrot::common
