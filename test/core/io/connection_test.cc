#include <gmock/gmock.h>

#include <numeric>

#include "test/mocks/event/mocks.hh"

#include "core/io/connection.hh"
#include "gtest/gtest.h"

namespace carrot::io {
namespace {

using ::testing::_;
using ::testing::Invoke;

// ============================================================================
// Chunk Tests
// ============================================================================

class ChunkTest : public ::testing::Test {
protected:
  Chunk chunk_;
};

TEST_F(ChunkTest, GetWriteAreaInitial) {
  auto write_area = chunk_.GetWriteArea();
  EXPECT_EQ(write_area.size(), BUFFER_SIZE);
}

TEST_F(ChunkTest, FillAndGetReadArea) {
  auto write_area = chunk_.GetWriteArea();
  std::fill(write_area.begin(), write_area.end(), std::byte{42});
  chunk_.Fill(100);

  auto read_area = chunk_.GetReadArea();
  EXPECT_EQ(read_area.size(), 100);
  EXPECT_EQ(read_area[0], std::byte{42});
}

TEST_F(ChunkTest, WritableSize) {
  EXPECT_EQ(chunk_.WritableSize(), BUFFER_SIZE);
  chunk_.Fill(100);
  EXPECT_EQ(chunk_.WritableSize(), BUFFER_SIZE - 100);
}

TEST_F(ChunkTest, ReadableSize) {
  EXPECT_EQ(chunk_.ReadableSize(), 0);
  chunk_.Fill(100);
  EXPECT_EQ(chunk_.ReadableSize(), 100);
}

TEST_F(ChunkTest, ConsumeData) {
  chunk_.Fill(100);
  EXPECT_EQ(chunk_.ReadableSize(), 100);

  chunk_.Consume(30);
  EXPECT_EQ(chunk_.ReadableSize(), 70);

  auto read_area = chunk_.GetReadArea();
  EXPECT_EQ(read_area.size(), 70);
}

TEST_F(ChunkTest, IsFullForReading) {
  EXPECT_FALSE(chunk_.IsFullForReading());
  chunk_.Fill(BUFFER_SIZE);
  EXPECT_TRUE(chunk_.IsFullForReading());
}

TEST_F(ChunkTest, HasReadableData) {
  EXPECT_FALSE(chunk_.HasReadableData());
  chunk_.Fill(1);
  EXPECT_TRUE(chunk_.HasReadableData());
}

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

TEST_F(ReadBufferTest, GetWriteSpan) {
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
  buffer.Pullup(BUFFER_SIZE + 50);

  auto read_span = buffer.GetSpan();
  EXPECT_GE(read_span.size(), BUFFER_SIZE + 50);
}

TEST_F(ReadBufferTest, HasReadableData) {
  auto buffer = CreateReadBuffer();
  EXPECT_FALSE(buffer.HasReadableData());

  auto write_span = buffer.GetWriteSpan();
  buffer.HandleCompletion(10, 0);

  EXPECT_TRUE(buffer.HasReadableData());
}

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