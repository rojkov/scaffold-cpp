#include "core/io/chunk.hh"
#include "gtest/gtest.h"

namespace carrot::io {
namespace {

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

} // namespace
} // namespace carrot::io
