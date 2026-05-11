#include <gmock/gmock.h>

#include "test/mocks/event/mocks.hh"

#include "core/io/connection.hh"
#include "gtest/gtest.h"

namespace carrot::io {
namespace {

using ::testing::_;

// ============================================================================
// Connection Tests
// ============================================================================

class ConnectionTest : public ::testing::Test {
protected:
  std::shared_ptr<carrot::event::MockDispatcher> dispatcher_ =
      std::make_shared<carrot::event::MockDispatcher>();
};

TEST_F(ConnectionTest, ConstructionSubmitsRead) {
  EXPECT_CALL(*dispatcher_, PrepareRead(_, -1, _, _)).Times(1);

  Connection conn(-1, dispatcher_);
}

TEST_F(ConnectionTest, SendQueuesWrite) {
  EXPECT_CALL(*dispatcher_, PrepareRead).Times(1);
  EXPECT_CALL(*dispatcher_, PrepareWrite).Times(1);

  Connection conn(-1, dispatcher_);

  std::vector<std::byte> data{std::byte{'H'}, std::byte{'i'}};
  conn.Send(std::span(data));
}

TEST_F(ConnectionTest, SendMultipleChunks) {
  EXPECT_CALL(*dispatcher_, PrepareRead).Times(1);
  EXPECT_CALL(*dispatcher_, PrepareWrite).Times(::testing::AtLeast(1));

  Connection conn(-1, dispatcher_);

  // Send data larger than BUFFER_SIZE
  std::vector<std::byte> large_data(BUFFER_SIZE * 2);
  conn.Send(std::span(large_data));
}

TEST_F(ConnectionTest, EchoResponseFormat) {
  EXPECT_CALL(*dispatcher_, PrepareRead).Times(::testing::AtLeast(1));

  Connection conn(-1, dispatcher_);

  // Without manually triggering dispatcher callbacks in a mock,
  // we can't test the full echo flow here.
  // This test verifies connection construction doesn't crash.
}

} // namespace
} // namespace carrot::io
