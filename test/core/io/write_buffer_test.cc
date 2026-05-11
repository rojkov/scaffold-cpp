#include "core/io/write_buffer.hh"
#include "gtest/gtest.h"

namespace carrot::io {
namespace {

// ============================================================================
// WriteBuffer Tests
// ============================================================================

class WriteBufferTest : public ::testing::Test {
protected:
  int completion_count_ = 0;
  int last_completion_res_ = 0;

  WriteBuffer CreateWriteBuffer() {
    return WriteBuffer{[this](WriteBuffer* /*buffer*/, int res) {
      completion_count_++;
      last_completion_res_ = res;
    }};
  }
};

TEST_F(WriteBufferTest, AppendAndGetPendingWrite) {
  auto buffer = CreateWriteBuffer();
  std::vector<std::byte> data(50);
  std::fill(data.begin(), data.end(), std::byte{42});

  buffer.Append(std::span(data));

  EXPECT_TRUE(buffer.HasPendingWrite());
  auto pending = buffer.GetPendingWriteSpan();
  EXPECT_EQ(pending.size(), 50);
  EXPECT_EQ(pending[0], std::byte{42});
}

TEST_F(WriteBufferTest, HandleCompletionDrains) {
  auto buffer = CreateWriteBuffer();
  std::vector<std::byte> data(100);
  for (size_t i = 0; i < data.size(); ++i) {
    data[i] = std::byte{static_cast<unsigned char>(i % 256)};
  }

  buffer.Append(std::span(data));
  EXPECT_EQ(buffer.GetPendingWriteSpan().size(), 100);

  buffer.HandleCompletion(60, 0);

  EXPECT_EQ(completion_count_, 1);
  EXPECT_EQ(last_completion_res_, 60);
  EXPECT_TRUE(buffer.HasPendingWrite());
  EXPECT_EQ(buffer.GetPendingWriteSpan().size(), 40);
}

TEST_F(WriteBufferTest, DrainAllData) {
  auto buffer = CreateWriteBuffer();
  std::vector<std::byte> data(50);
  buffer.Append(std::span(data));

  buffer.Drain(50);
  EXPECT_FALSE(buffer.HasPendingWrite());
}

TEST_F(WriteBufferTest, AppendLargeData) {
  auto buffer = CreateWriteBuffer();
  std::vector<std::byte> large_data(BUFFER_SIZE * 2 + 100);
  for (size_t i = 0; i < large_data.size(); ++i) {
    large_data[i] = std::byte{static_cast<unsigned char>(i % 256)};
  }

  buffer.Append(std::span(large_data));

  EXPECT_TRUE(buffer.HasPendingWrite());
  auto pending = buffer.GetPendingWriteSpan();
  EXPECT_LE(pending.size(), BUFFER_SIZE);
}

} // namespace
} // namespace carrot::io