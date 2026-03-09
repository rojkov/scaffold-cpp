#include <gtest/gtest.h>

#include "src/component_a/component.hh"

namespace carrot {
namespace {

TEST(ComponentTest, BasicAssertions) {
  Agent agent;

  ASSERT_EQ(agent.Run(), 1);
}

} // namespace
} // namespace carrot
