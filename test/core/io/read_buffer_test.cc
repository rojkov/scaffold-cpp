#include "core/io/read_buffer.hh"
#include "gtest/gtest.h"

namespace carrot::io {
namespace {

// ============================================================================
// ReadBuffer Tests
// ============================================================================

class ReadBufferTest : public ::testing::Test {
protected:
  int completion_count_ = 0;
  int last_completion_res_ = 0;

  ReadBuffer CreateReadBuffer() {
    return ReadBuffer{[this](ReadBuffer* /*buffer*/, int res) {
      completion_count_++;
      last_completion_res_ = res;
    }};
  }
};

TEST_F(ReadBufferTest, EmptyUponCreationReadBuffer) {
  auto buffer = CreateReadBuffer();
  auto span = buffer.GetSpan();
  EXPECT_TRUE(span.empty());
}

TEST_F(ReadBufferTest, GetWriteSpanForNewReadBuffer) {
  auto buffer = CreateReadBuffer();
  auto span = buffer.GetWriteSpan();
  EXPECT_EQ(span.size(), BUFFER_SIZE);
}

TEST_F(ReadBufferTest, HandleCompletionAndReadData) {
  auto buffer = CreateReadBuffer();
  auto write_span = buffer.GetWriteSpan();
  std::fill(write_span.begin(), write_span.end(), std::byte{123});

  buffer.HandleCompletion(100, 0);

  EXPECT_EQ(completion_count_, 1);
  EXPECT_EQ(last_completion_res_, 100);
  EXPECT_TRUE(buffer.HasReadableData());
  EXPECT_EQ(buffer.GetTotalReadableSize(), 100);
}

TEST_F(ReadBufferTest, GetSpan) {
  auto buffer = CreateReadBuffer();
  auto write_span = buffer.GetWriteSpan();
  for (int i = 0; i < 50; ++i) {
    write_span[i] = std::byte{static_cast<unsigned char>(i)};
  }
  buffer.HandleCompletion(50, 0);

  auto read_span = buffer.GetSpan();
  EXPECT_EQ(read_span.size(), 50);
  EXPECT_EQ(read_span[0], std::byte{0});
  EXPECT_EQ(read_span[49], std::byte{49});
}

TEST_F(ReadBufferTest, DrainData) {
  auto buffer = CreateReadBuffer();
  auto write_span = buffer.GetWriteSpan();
  std::fill(write_span.begin(), write_span.begin() + 100, std::byte{1});
  buffer.HandleCompletion(100, 0);

  EXPECT_EQ(buffer.GetTotalReadableSize(), 100);
  buffer.Drain(30);
  EXPECT_EQ(buffer.GetTotalReadableSize(), 70);
}

TEST_F(ReadBufferTest, MultipleChunks) {
  auto buffer = CreateReadBuffer();

  // Fill first chunk
  auto write_span1 = buffer.GetWriteSpan();
  std::fill(write_span1.begin(), write_span1.end(), std::byte{1});
  buffer.HandleCompletion(BUFFER_SIZE, 0);

  // Fill second chunk (partial)
  auto write_span2 = buffer.GetWriteSpan();
  std::fill(write_span2.begin(), write_span2.begin() + 100, std::byte{2});
  buffer.HandleCompletion(100, 0);

  EXPECT_EQ(buffer.GetTotalReadableSize(), BUFFER_SIZE + 100);
}

TEST_F(ReadBufferTest, PullupFromEmpty) {
  auto buffer = CreateReadBuffer();
  auto span = buffer.Pullup(100);
  EXPECT_TRUE(span.empty());
}

TEST_F(ReadBufferTest, PullupContiguous) {
  auto buffer = CreateReadBuffer();

  // Create two chunks with data
  auto write1 = buffer.GetWriteSpan();
  std::fill(write1.begin(), write1.end(), std::byte{1});
  buffer.HandleCompletion(BUFFER_SIZE, 0);

  auto write2 = buffer.GetWriteSpan();
  std::fill(write2.begin(), write2.begin() + 50, std::byte{2});
  buffer.HandleCompletion(50, 0);

  // Pullup should linearize across chunks
  auto read_span = buffer.Pullup(BUFFER_SIZE + 50);
  EXPECT_GE(read_span.size(), BUFFER_SIZE + 50);
}

TEST_F(ReadBufferTest, HasReadableData) {
  auto buffer = CreateReadBuffer();
  EXPECT_FALSE(buffer.HasReadableData());

  [[maybe_unused]] auto write_span = buffer.GetWriteSpan();
  buffer.HandleCompletion(10, 0);

  EXPECT_TRUE(buffer.HasReadableData());
}

} // namespace
} // namespace carrot::io
